#ifndef TORCLOGGINGIMP_H
#define TORCLOGGINGIMP_H

#define LOGLINE_MAX (2048-120)

// Qt
#include <QFile>

// Torc
#include "torchttpservice.h"

class LogItem;

void RegisterLoggingThread(void);
void DeregisterLoggingThread(void);

class LoggerBase
{
  public:
    explicit LoggerBase(QString FileName);
    virtual ~LoggerBase();

    virtual bool Logmsg(LogItem *Item) = 0;

  protected:
    virtual bool PrintLine (QByteArray &Line) = 0;
    QByteArray   GetLine(LogItem *Item);

  protected:
    QString      m_fileName;
};

class FileLogger : public LoggerBase
{
  public:
    FileLogger(QString Filename, bool ErrorsOnly, int Quiet);
   ~FileLogger();

    bool   Logmsg(LogItem *Item) Q_DECL_OVERRIDE;

  protected:
    bool   PrintLine (QByteArray &Line) Q_DECL_OVERRIDE;

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
    Q_CLASSINFO("GetLog",  "type=Log")
    Q_CLASSINFO("GetTail", "type=Logline")
    Q_PROPERTY(QByteArray log  READ GetLog  NOTIFY logChanged)
    Q_PROPERTY(QByteArray tail READ GetTail NOTIFY tailChanged)

  public:
    WebLogger(QString Filename);
   ~WebLogger();

    bool Logmsg (LogItem *Item) Q_DECL_OVERRIDE;

  public slots:
    bool       event             (QEvent *event) Q_DECL_OVERRIDE;
    void       SubscriberDeleted (QObject *Subscriber);
    QByteArray GetLog            (void);
    QByteArray GetTail           (void);

  signals:
    void       logChanged        (bool Updated);
    void       tailChanged       (QByteArray Tail);

  private:
    QByteArray log;
    QByteArray tail;
    bool       changed;
    QMutex     lock;
};

#endif // TORCLOGGINGIMP_H
