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
    Q_PROPERTY(QString userName    MEMBER m_userName        READ GetUserName       CONSTANT)

    friend class TorcHTTPServer;

  public:
    TorcUser();
   ~TorcUser();

    static QByteArray GetCredentials    (void);
    static QString    GetName           (void);

  protected slots:
    void           UpdateUserName       (QString &Name);
    void           UpdateCredentials    (QString &Credentials);

  public slots:
    // TorcHTTPService
    void           SubscriberDeleted    (QObject *Subscriber);

    QString        GetUserName          (void);
    bool           SetUserCredentials   (const QString &Name, const QString &Credentials);
    bool           GetCanRestartTorc    (void);
    void           RestartTorc          (void);
    void           StopTorc             (void);
    bool           GetCanStopTorc       (void);

  public:
    static QString    gUserName;
    static QByteArray gUserCredentials;
    static QMutex     gUserCredentialsLock;

  private:
    Q_DISABLE_COPY(TorcUser)

    QString        m_userName;
    TorcSetting   *m_userNameSetting;
    TorcSetting   *m_userCredentials;
    bool           m_canRestartTorc;
    bool           m_canStopTorc;
};

#endif // TORCUSER_H
