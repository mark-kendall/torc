#ifndef TORCUSER_H
#define TORCUSER_H

// Qt
#include <QObject>

// Torc
#include "torcsetting.h"
#include "torchttpservice.h"

class TorcUser : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",     "1.0.0")
    Q_CLASSINFO("RestartTorc", "methods=PUT")
    Q_CLASSINFO("StopTorc",    "methods=PUT")
    Q_PROPERTY(bool canRestartTorc MEMBER m_canRestartTorc  READ GetCanRestartTorc CONSTANT)
    Q_PROPERTY(bool canStopTorc    MEMBER m_canStopTorc     READ GetCanStopTorc    CONSTANT)

    friend class TorcHTTPServer;

  public:
    TorcUser();
   ~TorcUser();

    static QByteArray GetCredentials    (void);

  protected slots:
    void           UpdateCredentials    (const QString &Credentials);

  public slots:
    // TorcHTTPService
    void           SubscriberDeleted    (QObject *Subscriber);

    bool           SetUserCredentials   (const QString &Credentials);
    bool           GetCanRestartTorc    (void);
    void           RestartTorc          (void);
    void           StopTorc             (void);
    bool           GetCanStopTorc       (void);

  public:
    static QByteArray gCredentials;
    static QMutex     gCredentialsLock;

  private:
    Q_DISABLE_COPY(TorcUser)

    TorcSetting   *m_user;
    bool           m_canRestartTorc;
    bool           m_canStopTorc;
    QMutex         m_lock;

};

#endif // TORCUSER_H
