#ifndef TORCLOGGINGIMP_H
#define TORCLOGGINGIMP_H

#define LOGLINE_MAX (2048-120)

// Qt
#include <QFile>
#include <QWaitCondition>

// Torc
#include "torcqthread.h"
#include "torchttpservice.h"

class LogItem;

void RegisterLoggingThread(void);
void DeregisterLoggingThread(void);

class LoggingThread : public TorcQThread
{
    Q_OBJECT
  public:
    LoggingThread();
   ~LoggingThread();

    // TorcQThread
    void run(void);
    void Start(void);
    void Finish(void);
    void Stop(void);
    bool Flush(int TimeoutMS = 200000);
    void HandleItem(LogItem *Item);

  private:
    QWaitCondition m_waitNotEmpty;
    QWaitCondition m_waitEmpty;
    bool           m_aborted;
};

class LoggerBase
{
  public:
    explicit LoggerBase(const QString &FileName);
    virtual ~LoggerBase() = default;

    virtual bool Logmsg(LogItem *Item) = 0;

  protected:
    virtual bool PrintLine (QByteArray &Line) = 0;
    QByteArray   GetLine(LogItem *Item);

  protected:
    QString      m_fileName;

  private:
    Q_DISABLE_COPY(LoggerBase)
};

class FileLogger : public LoggerBase
{
  public:
    FileLogger(const QString &Filename, bool ErrorsOnly, int Quiet);
   ~FileLogger();

    bool   Logmsg(LogItem *Item) override;

  protected:
    bool   PrintLine (QByteArray &Line) override;

  protected:
    bool   m_opened;
    QFile  m_file;
    bool   m_errorsOnly;
    int    m_quiet;
};

class WebLogger : public QObject, public TorcHTTPService, public FileLogger
{
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("GetLog",  "type=Log,methods=GET+AUTH")
    Q_CLASSINFO("GetTail", "type=Logline,methods=GET+AUTH")
    Q_PROPERTY(QByteArray log  READ GetLog  NOTIFY logChanged)
    Q_PROPERTY(QByteArray tail READ GetTail NOTIFY tailChanged)

  public:
    explicit WebLogger(const QString &Filename);
   ~WebLogger() = default;

    bool Logmsg (LogItem *Item) override;

  public slots:
    bool       event             (QEvent *event) override;
    void       SubscriberDeleted (QObject *Subscriber);
    QByteArray GetLog            (void);
    QByteArray GetTail           (void);

  signals:
    void       logChanged        (void);
    void       tailChanged       (void);

  private:
    QByteArray log;
    QByteArray tail;
    bool       changed;
};

#endif // TORCLOGGINGIMP_H
