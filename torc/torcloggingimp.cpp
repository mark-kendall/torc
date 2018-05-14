// Std
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <iostream>

// Qt
#include <QtGlobal>
#include <QMutex>
#include <QList>
#include <QTime>
#include <QRegExp>
#include <QHash>
#include <QMap>
#include <QByteArray>
#include <QWaitCondition>
#include <QFileInfo>
#include <QStringList>
#include <QQueue>

// Torc
#include "torcexitcodes.h"
#include "torccompat.h"
#include "torcqthread.h"
#include "torclogging.h"
#include "torcloggingimp.h"

// Various ways to get to thread's tid
#ifdef Q_OS_LINUX
#include <unistd.h>
#include <sys/syscall.h>
#elif defined(__FreeBSD__)
extern "C" {
#include <sys/ucontext.h>
#include <sys/thr.h>
}
#elif defined(Q_OS_MAC)
#include <mach/mach.h>
#endif

class LoggingThread;
class LogItem;

using namespace std;

QMutex                   gLoggerListLock;
QList<LoggerBase *>      gLoggerList;
QMutex                   gLogQueueLock;
QQueue<LogItem *>        gLogQueue;
QRegExp                  gLogRexExp = QRegExp("[%]{1,2}");
QMutex                   gLogThreadLock;
QHash<uint64_t, char *>  gLogThreadHash;
QMutex                   gLogThreadTidLock;
QHash<uint64_t, int64_t> gLogThreadtidHash;
LoggingThread           *gLogThread = NULL;
bool                     gLogThreadFinished = false;
bool                     gDebugRegistration = false;

typedef enum {
    kMessage       = 0x01,
    kRegistering   = 0x02,
    kDeregistering = 0x04,
    kFlush         = 0x08,
    kStandardIO    = 0x10,
} LoggingType;

char*   GetThreadName(LogItem *item);
int64_t GetThreadTid(LogItem *item);

class LogItem
{
  public:
    static LogItem* Create(const char* File, const char* Function,
                           int Line, LogLevel Level, int Type)
    {
        return new LogItem(File, Function, Line, Level, Type);
    }

    static void Delete(LogItem *Item)
    {
        if (Item && !Item->refCount.deref())
        {
            if (Item->threadName)
                free(Item->threadName);
            delete Item;
        }
    }

    LogItem(const char *File, const char *Function,
            int Line, LogLevel Level, int Type)
      : refCount(),
        threadId((uint64_t)(QThread::currentThreadId())),
        usec(0),
        line(Line),         type(Type),
        level(Level),
        tm(),
        file(File),
        function(Function), threadName(NULL)
    {
        SetTime();
        SetThreadTid();

        message[0]='\0';
        message[LOGLINE_MAX]='\0';

        refCount.ref();
    }

    void SetTime(void)
    {
        time_t epoch;

#if HAVE_GETTIMEOFDAY
        struct timeval  tv;
        gettimeofday(&tv, NULL);
        epoch = tv.tv_sec;
        usec  = tv.tv_usec;
#else
        QDateTime date = QDateTime::currentDateTime();
        QTime     time = date.time();
        epoch = date.toTime_t();
        usec = time.msec() * 1000;
#endif

        localtime_r(&epoch, &tm);
    }

    void SetThreadTid(void)
    {
        QMutexLocker locker(&gLogThreadTidLock);

        if (!gLogThreadtidHash.contains(this->threadId))
        {
            int64_t tid = 0;

#ifdef Q_OS_LINUX
            tid = (int64_t)syscall(SYS_gettid);
#elif defined(__FreeBSD__)
            long lwpid;
            int dummy = thr_self( &lwpid );
            (void)dummy;
            tid = (int64_t)lwpid;
#elif defined(Q_OS_MAC)
            tid = (int64_t)mach_thread_self();
#endif
            gLogThreadtidHash[this->threadId] = tid;
        }
    }

    QAtomicInt          refCount;
    uint64_t            threadId;
    uint32_t            usec;
    int                 line;
    int                 type;
    LogLevel            level;
    struct tm           tm;
    const char         *file;
    const char         *function;
    char               *threadName;
    char                message[LOGLINE_MAX+1];
};

class LoggingThread : public TorcQThread
{
  public:
    LoggingThread();
   ~LoggingThread();

    // TorcQThread
    void run(void)
    {
        Initialise();

        gLogThreadFinished = false;

        QMutexLocker lock(&gLogQueueLock);

        while (!m_aborted || !gLogQueue.isEmpty())
        {
            if (gLogQueue.isEmpty())
            {
                m_waitEmpty.wakeAll();
                m_waitNotEmpty.wait(lock.mutex(), 100);
                continue;
            }

            LogItem *item = gLogQueue.dequeue();
            lock.unlock();

            HandleItem(item);
            LogItem::Delete(item);

            lock.relock();
        }

        gLogThreadFinished = true;

        lock.unlock();

        Deinitialise();
    }

    void Start(void)
    {
    }

    void Finish(void)
    {
    }

    void Stop(void)
    {
        if (m_aborted)
            return;

        {
            QMutexLocker lock(&gLogQueueLock);
            Flush(1000);
            m_aborted = true;
        }
        m_waitNotEmpty.wakeAll();
    }

    bool Flush(int TimeoutMS = 200000)
    {
        QTime t;
        t.start();
        while (!m_aborted && !gLogQueue.isEmpty() && t.elapsed() < TimeoutMS)
        {
            m_waitNotEmpty.wakeAll();
            int left = TimeoutMS - t.elapsed();
            if (left > 0)
                m_waitEmpty.wait(&gLogQueueLock, left);
        }
        return gLogQueue.isEmpty();
    }

    void HandleItem(LogItem *Item)
    {
        if (Item->type & kRegistering)
        {
            int64_t tid = GetThreadTid(Item);

            QMutexLocker locker(&gLogThreadLock);
            if (gLogThreadHash.contains(Item->threadId))
                free(gLogThreadHash.take(Item->threadId));

            gLogThreadHash[Item->threadId] = strdup(Item->threadName);

            if (gDebugRegistration)
            {
                snprintf(Item->message, LOGLINE_MAX,
                         "Thread 0x%" PREFIX64 "X (%" PREFIX64
                         "d) registered as \'%s\'",
                         (long long unsigned int)Item->threadId,
                         (long long int)tid,
                         gLogThreadHash[Item->threadId]);
            }
        }
        else if (Item->type & kDeregistering)
        {
            int64_t tid = 0;
            {
                QMutexLocker locker(&gLogThreadTidLock);
                if (gLogThreadtidHash.contains(Item->threadId))
                {
                    tid = gLogThreadtidHash[Item->threadId];
                    gLogThreadtidHash.remove(Item->threadId);
                }
            }

            QMutexLocker locker(&gLogThreadLock);
            if (gLogThreadHash.contains(Item->threadId))
            {
                if (gDebugRegistration)
                {
                    snprintf(Item->message, LOGLINE_MAX,
                             "Thread 0x%" PREFIX64 "X (%" PREFIX64
                             "d) deregistered as \'%s\'",
                             (long long unsigned int)Item->threadId,
                             (long long int)tid,
                             gLogThreadHash[Item->threadId]);
                }
                char* threadname = gLogThreadHash.take(Item->threadId);
                free(threadname);
            }
        }

        if (Item->message[0] != '\0')
        {
            QMutexLocker locker(&gLoggerListLock);

            QList<LoggerBase *>::iterator it;
            for (it = gLoggerList.begin(); it != gLoggerList.end(); ++it)
                (*it)->Logmsg(Item);
        }
    }

  private:
    QWaitCondition m_waitNotEmpty;
    QWaitCondition m_waitEmpty;
    bool           m_aborted;
};

typedef struct {
    bool    propagate;
    int     quiet;
    QString path;
} LogPropagateOpts;

LogPropagateOpts gLogPropagationOpts = { false, 0, QString("") };
QString          gLogPropagationArgs;

#define TIMESTAMP_MAX 30
#define MAX_STRING_LENGTH (LOGLINE_MAX+120)

LogLevel gLogLevel = (LogLevel)LOG_INFO;

typedef struct {
    uint64_t mask      { 0 };
    QString  name      { QString("") };
    bool     additive  { false };
    QString  helpText  { QString("") };
} VerboseDef;

typedef QMap<QString, VerboseDef> VerboseMap;

typedef struct {
    int         value     { 0 };
    QString     name      { QString("") };
    char        shortname { '?' };
} LoglevelDef;

typedef QMap<int, LoglevelDef> LoglevelMap;

VerboseMap     gVerboseMap;
QMutex         gVerboseMapLock;
LoglevelMap    gLoglevelMap;
QMutex         gLoglevelMapLock;

bool           gVerboseInitialised    = false;
const uint64_t gVerboseDefaultInt     = VB_GENERAL;
const char    *gVerboseDefaultStr     = " general";
uint64_t       gVerboseMask           = gVerboseDefaultInt;
QString        gVerboseString         = QString(gVerboseDefaultStr);
uint64_t       gUserDefaultValueInt   = gVerboseDefaultInt;
QString        gUserDefaultValueStr   = QString(gVerboseDefaultStr);
bool           gHaveUserDefaultValues = false;

void AddVerbose(uint64_t mask, QString name, bool additive, QString helptext);
void AddLogLevel(int value, QString name, char shortname);
void InitVerbose(void);
void VerboseHelp(void);

LoggerBase::LoggerBase(QString FileName) : m_fileName(FileName)
{
    QMutexLocker locker(&gLoggerListLock);
    gLoggerList.append(this);
}

LoggerBase::~LoggerBase()
{
}

QByteArray LoggerBase::GetLine(LogItem *Item)
{
    if (!Item)
        return QByteArray();

    Item->refCount.ref();

    QByteArray line(MAX_STRING_LENGTH, ' ');
    //char line[MAX_STRING_LENGTH];
    char timestamp[TIMESTAMP_MAX];
    char usPart[9];
    strftime(timestamp, TIMESTAMP_MAX-8, "%Y-%m-%d %H:%M:%S",
             (const struct tm *)&Item->tm );
    snprintf(usPart, 5, ".%03d", (int)(Item->usec));
    strcat(timestamp, usPart);
    char shortname;

    {
        QMutexLocker locker(&gLoglevelMapLock);
        LoglevelMap::iterator it = gLoglevelMap.find(Item->level);
        if (it == gLoglevelMap.end())
            shortname = '-';
        else
            shortname = (*it).shortname;
    }

    if (Item->type & kStandardIO)
    {
        snprintf(line.data(), MAX_STRING_LENGTH, "%s", Item->message);
    }
    else
    {
        char fileline[50];
        snprintf(fileline, 50, "%s (%s:%d)", Item->function, Item->file, Item->line);
        char* threadName = GetThreadName(Item);
        pid_t tid = GetThreadTid(Item);

        snprintf(line.data(), MAX_STRING_LENGTH, "%s %c [%6d/%6d] %-11s %-50s - %s\n",
                 timestamp, shortname, getpid(), tid, threadName, fileline,
                 Item->message);
    }

    Item->refCount.deref();
    return line.trimmed();
}

FileLogger::FileLogger(QString Filename, bool ErrorsOnly, int Quiet)
  : LoggerBase(Filename),
    m_opened(false),
    m_file(),
    m_errorsOnly(ErrorsOnly),
    m_quiet(Quiet)
{
    if (m_fileName.isEmpty())
    {
        m_opened = true;
        LOG(VB_GENERAL, LOG_INFO, "Logging to the console");
    }
    else
    {
        if (QFile::exists(m_fileName))
        {
            QString old = m_fileName + ".old";

            LOG(VB_GENERAL, LOG_INFO, QString("Moving '%1' to '%2'")
                .arg(m_fileName).arg(old));

            QFile::remove(old);
            QFile::rename(m_fileName, old);
        }

        m_errorsOnly = false;
        m_quiet    = false;
        m_file.setFileName(m_fileName);
        m_opened = m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Truncate |
                               QIODevice::Text | QIODevice::Unbuffered);
        LOG(VB_GENERAL, LOG_INFO, QString("Logging to '%1'").arg(m_fileName));
    }
}

FileLogger::~FileLogger()
{
    if (m_opened)
    {
        LogItem *item = LogItem::Create(__FILE__, __FUNCTION__,
                                        __LINE__, LOG_INFO, kMessage);

        strcpy(item->message, m_file.isOpen() ? "Closing file logger." : "Closing console logger.");
        Logmsg(item);
        LogItem::Delete(item);
        //m_file.flush();
        m_file.close();
    }
}

bool FileLogger::Logmsg(LogItem *Item)
{
    if (!m_opened || m_quiet || (m_errorsOnly && (Item->level > LOG_ERR)))
        return false;

    Item->refCount.ref();
    QByteArray line = GetLine(Item);
    LogItem::Delete(Item);
    return PrintLine(line);
}

bool FileLogger::PrintLine(QByteArray &Line)
{
    int error = m_file.isOpen() ? m_file.write(Line) : write(1, Line, Line.size() /*strlen(line)*/);
    if (error == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
             QString("Closed Log output due to errors"));
        m_opened = false;
        return false;
    }

    return true;
}

WebLogger::WebLogger(QString Filename)
  : QObject(),
    TorcHTTPService(this, "log", "log", WebLogger::staticMetaObject, "event"),
    FileLogger(Filename, false, false),
    log(),
    tail(),
    changed(false),
    lock(QMutex::Recursive)
{
    // we rate limit the LogChanged signal to once every 10 seconds
    startTimer(10000);
}

WebLogger::~WebLogger()
{
}

bool WebLogger::event(QEvent *event)
{
    if (event && event->type() == QEvent::Timer)
    {
        lock.lock();
        if (changed)
            emit logChanged();
        changed = false;
        lock.unlock();
    }
    return QObject::event(event);
}

void WebLogger::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

QByteArray WebLogger::GetLog(void)
{
    QFile log(m_file.fileName());
    if (!log.open(QIODevice::ReadOnly))
        return QByteArray();
    QByteArray result = log.readAll();
    log.close();
    return result;
}

QByteArray WebLogger::GetTail(void)
{
    QMutexLocker locker(&lock);
    return tail;
}

bool WebLogger::Logmsg(LogItem *Item)
{
    if (!m_opened || m_quiet || (m_errorsOnly && (Item->level > LOG_ERR)))
        return false;

    Item->refCount.ref();
    QByteArray line = GetLine(Item);
    LogItem::Delete(Item);

    lock.lock();
    changed = true;
    tail = line;
    lock.unlock();

    emit tailChanged();
    return PrintLine(line);
}

char *GetThreadName(LogItem *Item)
{
    static const char *unknown = "QRunnable";

    if (!Item)
        return (char*)unknown;

    char *threadName;

    if (!Item->threadName)
    {
        QMutexLocker locker(&gLogThreadLock);
        if (gLogThreadHash.contains(Item->threadId))
            threadName = gLogThreadHash[Item->threadId];
        else
            threadName = (char*)unknown;
    }
    else
    {
        threadName = Item->threadName;
    }

    return threadName;
}

int64_t GetThreadTid(LogItem *Item)
{
    if (!Item)
        return 0;

    pid_t tid = 0;

    QMutexLocker locker(&gLogThreadTidLock);
    if (gLogThreadtidHash.contains(Item->threadId))
        tid = gLogThreadtidHash[Item->threadId];

    return(tid);
}

LoggingThread::LoggingThread()
  : TorcQThread("Logger"),
    m_waitNotEmpty(),
    m_waitEmpty(),
    m_aborted(false)
{
    if (!qgetenv("VERBOSE_THREADS").isEmpty())
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            "Logging thread registration/deregistration enabled!");
        gDebugRegistration = true;
    }
}

LoggingThread::~LoggingThread()
{
    Stop();
    quit();
    wait();
}

void PrintLogLine(uint64_t Mask, LogLevel Level, const char *File, int Line,
                  const char *Function, int FromQString,
                  const char *Format, ... )
{
    int type = kMessage;
    type |= (Mask & VB_FLUSH) ? kFlush : 0;
    type |= (Mask & VB_STDIO) ? kStandardIO : 0;
    LogItem *item = LogItem::Create(File, Function, Line, Level, type);
    if (!item)
        return;

    char* copy = NULL;
    if (FromQString && strchr(Format, '%'))
    {
        QString string(Format);
        Format = strdup(string.replace(gLogRexExp, "%%").toLocal8Bit().constData());
        copy = (char*)Format;
    }

    va_list arguments;
    va_start(arguments, Format);
    vsnprintf(item->message, LOGLINE_MAX, Format, arguments);
    va_end(arguments);

    if (copy)
        free(copy);

    QMutexLocker lock(&gLogQueueLock);

    gLogQueue.enqueue(item);

    if (gLogThread && gLogThreadFinished && !gLogThread->isRunning())
    {
        while (!gLogQueue.isEmpty())
        {
            item = gLogQueue.dequeue();
            lock.unlock();
            gLogThread->HandleItem(item);
            LogItem::Delete(item);
            lock.relock();
        }
    }
    else if (gLogThread && !gLogThreadFinished && (type & kFlush))
    {
        gLogThread->Flush();
    }
}

void CalculateLogPropagation(void)
{
    QString mask = gVerboseString.trimmed();
    mask.replace(QRegExp(" "), ",");
    mask.remove(QRegExp("^,"));
    gLogPropagationArgs = " --verbose " + mask;

    if (gLogPropagationOpts.propagate)
        gLogPropagationArgs += " --logpath " + gLogPropagationOpts.path;

    QString name = GetLogLevelName(gLogLevel);
    gLogPropagationArgs += " --loglevel " + name;

    for (int i = 0; i < gLogPropagationOpts.quiet; i++)
        gLogPropagationArgs += " --quiet";
}

bool GetQuietLogPropagation(void)
{
    return gLogPropagationOpts.quiet;
}

void StartLogging(QString Logfile, int progress, int quiet,
                  QString Level, bool Propagate)
{
    RegisterLoggingThread();

    LogLevel level = GetLogLevel(Level);

    {
        QMutexLocker lock(&gLogQueueLock);
        if (!gLogThread)
            gLogThread = new LoggingThread();
        if (!gLogThread)
            return;
    }

    if (gLogThread->isRunning())
        return;

    gLogLevel = level;
    LOG(VB_GENERAL, LOG_NOTICE, QString("Setting level to LOG_%1")
             .arg(GetLogLevelName(gLogLevel).toUpper()));

    gLogPropagationOpts.propagate = Propagate;
    gLogPropagationOpts.quiet = quiet;

    if (Propagate)
    {
        QFileInfo finfo(Logfile);
        QString path = finfo.path();
        gLogPropagationOpts.path = path;
    }

    CalculateLogPropagation();

    new FileLogger(QString(""), progress, quiet);

    if (!Logfile.isEmpty())
        new WebLogger(Logfile);

    gLogThread->start();
}

void StopLogging(void)
{
    if (gLogThread)
    {
        gLogThread->Stop();
        gLogThread->quit();
        gLogThread->wait();
    }

    {
        QMutexLocker lock(&gLogQueueLock);
        delete gLogThread;
        gLogThread = NULL;
    }

    {
        QMutexLocker locker(&gLoggerListLock);
        foreach (LoggerBase* logger, gLoggerList)
            delete logger;
    }

    // this cleans up the last thread name - the logging thread itself - which is deregistered
    // after the logging thread runloop has finished...
    {
        QMutexLocker locker(&gLogThreadLock);
        QHash<uint64_t, char *>::iterator it = gLogThreadHash.begin();
        for ( ; it != gLogThreadHash.end(); ++it)
            free(it.value());
    }
}

void RegisterLoggingThread(void)
{
    if (gLogThreadFinished)
        return;

    QMutexLocker lock(&gLogQueueLock);

    LogItem *item = LogItem::Create(__FILE__, __FUNCTION__, __LINE__,
                                   (LogLevel)LOG_DEBUG, kRegistering);
    if (item)
    {
        item->threadName = strdup((char *)QThread::currentThread()->objectName().toLocal8Bit().constData());
        gLogQueue.enqueue(item);
    }
}

void DeregisterLoggingThread(void)
{
    if (gLogThreadFinished)
        return;

    QMutexLocker lock(&gLogQueueLock);

    LogItem *item = LogItem::Create(__FILE__, __FUNCTION__, __LINE__,
                                   (LogLevel)LOG_DEBUG, kDeregistering);
    if (item)
        gLogQueue.enqueue(item);
}

LogLevel GetLogLevel(QString level)
{
    QMutexLocker locker(&gLoglevelMapLock);
    if (!gVerboseInitialised)
    {
        locker.unlock();
        InitVerbose();
        locker.relock();
    }

    for (LoglevelMap::iterator it = gLoglevelMap.begin();
         it != gLoglevelMap.end(); ++it)
    {
        if ((*it).name == level.toLower() )
            return (LogLevel)(*it).value;
    }

    return LOG_UNKNOWN;
}

QString GetLogLevelName(LogLevel level)
{
    QMutexLocker locker(&gLoglevelMapLock);
    if (!gVerboseInitialised)
    {
        locker.unlock();
        InitVerbose();
        locker.relock();
    }
    LoglevelMap::iterator it = gLoglevelMap.find((int)level);

    if ( it == gLoglevelMap.end() )
        return QString("unknown");

    return (*it).name;
}

void AddVerbose(uint64_t mask, QString name, bool additive, QString helptext)
{
    VerboseDef item;

    item.mask = mask;
    name.detach();
    // VB_GENERAL -> general
    name.remove(0, 3);
    name = name.toLower();
    item.name = name;
    item.additive = additive;
    helptext.detach();
    item.helpText = helptext;

    gVerboseMap.insert(name, item);
}

void AddLogLevel(int value, QString name, char shortname)
{
    LoglevelDef item;

    item.value = value;
    name.detach();
    // LOG_CRIT -> crit
    name.remove(0, 4);
    name = name.toLower();
    item.name = name;
    item.shortname = shortname;

    gLoglevelMap.insert(value, item);
}

void InitVerbose(void)
{
    QMutexLocker locker(&gVerboseMapLock);
    QMutexLocker locker2(&gLoglevelMapLock);
    gVerboseMap.clear();
    gLoglevelMap.clear();

#undef TORCLOGGINGDEFS_H_
#define _IMPLEMENT_VERBOSE
#include "torcloggingdefs.h"

    gVerboseInitialised = true;
}

void VerboseHelp(void)
{
    QString m_verbose = gVerboseString.trimmed();
    m_verbose.replace(QRegExp(" "), ",");
    m_verbose.remove(QRegExp("^,"));

    cerr << "Verbose debug levels.\n"
            "Accepts any combination (separated by comma) of:\n\n";

    for (VerboseMap::Iterator vit = gVerboseMap.begin();
         vit != gVerboseMap.end(); ++vit )
    {
        QString name = QString("  %1").arg(vit.value().name, -15, ' ');
        if (vit.value().helpText.isEmpty())
            continue;
        cerr << name.toLocal8Bit().constData() << " - " <<
                vit.value().helpText.toLocal8Bit().constData() << endl;
    }

    cerr << endl <<
      "The default for this program appears to be: '-v " <<
      m_verbose.toLocal8Bit().constData() << "'\n\n"
      "Most options are additive except for 'none' and 'all'.\n"
      "These two are semi-exclusive and take precedence over any\n"
      "other options.  However, you may use something like\n"
      "'-v none,jobqueue' to receive only JobQueue related messages\n"
      "and override the default verbosity level.\n\n"
      "Additive options may also be subtracted from 'all' by\n"
      "prefixing them with 'no', so you may use '-v all,nodatabase'\n"
      "to view all but database debug messages.\n\n"
      "Some debug levels may not apply to this program.\n\n";
}

int ParseVerboseArgument(QString arg)
{
    QString option;

    if (!gVerboseInitialised)
        InitVerbose();

    QMutexLocker locker(&gVerboseMapLock);

    gVerboseMask   = gVerboseDefaultInt;
    gVerboseString = QString(gVerboseDefaultStr);

    if (arg.startsWith('-'))
    {
        cerr << "Invalid or missing argument to -v/--verbose option\n";
        return TORC_EXIT_INVALID_CMDLINE;
    }

    QStringList verboseOpts = arg.split(QRegExp("\\W+"));
    for (QStringList::Iterator it = verboseOpts.begin();
         it != verboseOpts.end(); ++it )
    {
        option = (*it).toLower();
        bool reverseOption = false;

        if (option != "none" && option.left(2) == "no")
        {
            reverseOption = true;
            option = option.right(option.length() - 2);
        }

        if (option == "help")
        {
            VerboseHelp();
            return TORC_EXIT_INVALID_CMDLINE;
        }
        else if (option == "important")
        {
            cerr << "The \"important\" log mask is no longer valid.\n";
        }
        else if (option == "extra")
        {
            cerr << "The \"extra\" log mask is no longer valid.  Please try "
                    "--loglevel debug instead.\n";
        }
        else if (option == "default")
        {
            if (gHaveUserDefaultValues)
            {
                gVerboseMask   = gUserDefaultValueInt;
                gVerboseString = gUserDefaultValueStr;
            }
            else
            {
                gVerboseMask   = gVerboseDefaultInt;
                gVerboseString = QString(gVerboseDefaultStr);
            }
        }
        else
        {
            if (gVerboseMap.contains(option))
            {
                VerboseDef item = gVerboseMap.value(option);
                if (reverseOption)
                {
                    gVerboseMask &= ~(item.mask);
                    gVerboseString = gVerboseString.remove(' ' + item.name);
                    gVerboseString += " no" + item.name;
                }
                else
                {
                    if (item.additive)
                    {
                        if (!(gVerboseMask & item.mask))
                        {
                            gVerboseMask |= item.mask;
                            gVerboseString += ' ' + item.name;
                        }
                    }
                    else
                    {
                        gVerboseMask = item.mask;
                        gVerboseString = item.name;
                    }
                }
            }
            else
            {
                cerr << "Unknown argument for -v/--verbose: " <<
                        option.toLocal8Bit().constData() << endl;;
                return TORC_EXIT_INVALID_CMDLINE;
            }
        }
    }

    if (!gHaveUserDefaultValues)
    {
        gHaveUserDefaultValues = true;
        gUserDefaultValueInt   = gVerboseMask;
        gUserDefaultValueStr   = gVerboseString;
    }

    return TORC_EXIT_OK;
}

QString LogErrorToString(int errnum)
{
    return QString("%1 (%2)").arg(strerror(errnum)).arg(errnum);
}

// vim:ts=4:sw=4:ai:et:si:sts=4

