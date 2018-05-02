#ifndef TORCNETWORK_H
#define TORCNETWORK_H

// Qt
#include <QObject>
#include <QtNetwork>
#include <QAtomicInt>

// Torc
#include "torcsetting.h"
#include "torctimer.h"
#include "torcnetworkrequest.h"

#define DEFAULT_MAC_ADDRESS QString("00:00:00:00:00:00")
#define DEFAULT_STREAMED_BUFFER_SIZE (1024 * 1024 * 10)
#define DEFAULT_STREAMED_READ_SIZE   (32768)
#define DEFAULT_MAX_REDIRECTIONS     3
#define DEFAULT_USER_AGENT           QByteArray("Wget/1.12 (linux-gnu))")

class TorcNetworkRequest;

class TorcNetwork : QNetworkAccessManager
{
    friend class TorcNetworkObject;

    Q_OBJECT

  public:
    // Public API
    static bool IsAvailable         (void);
    static bool IsOwnAddress        (const QHostAddress &Address);
    static bool Get                 (TorcNetworkRequest* Request);
    static void Cancel              (TorcNetworkRequest* Request);
    static void Poke                (TorcNetworkRequest* Request);
    static bool GetAsynchronous     (TorcNetworkRequest* Request, QObject *Parent);
    static void AddHostName         (const QString &Host);
    static void RemoveHostName      (const QString &Host);
    static QStringList GetHostNames (void);

    static QString IPAddressToLiteral (const QHostAddress& Address, int Port, bool UseLocalhost = true);
    static bool IsExternal          (const QHostAddress &Address);
    static bool IsLinkLocal         (const QHostAddress &Address);
    static bool IsLocal             (const QHostAddress &Address);
    static bool IsGlobal            (const QHostAddress &Address);

  signals:
    void        NewRequest          (TorcNetworkRequest* Request);
    void        CancelRequest       (TorcNetworkRequest* Request);
    void        PokeRequest         (TorcNetworkRequest* Request);
    void        NewAsyncRequest     (TorcNetworkRequest* Request, QObject *Parent);

  protected slots:
    // QNetworkConfigurationManager
    void    ConfigurationAdded      (const QNetworkConfiguration &Config);
    void    ConfigurationChanged    (const QNetworkConfiguration &Config);
    void    ConfigurationRemoved    (const QNetworkConfiguration &Config);
    void    OnlineStateChanged      (bool  Online);
    void    UpdateCompleted         (void);

    // QNetworkReply
    void    ReadyRead               (void);
    void    Finished                (void);
    void    Error                   (QNetworkReply::NetworkError Code);
    void    SSLErrors               (const QList<QSslError> &Errors);
    void    DownloadProgress        (qint64 Received, qint64 Total);

    // QNetworkAccessManager
    void    Authenticate            (QNetworkReply* Reply, QAuthenticator* Authenticator);

    // QHostInfo
    void    NewHostName             (const QHostInfo &Info);

  private slots:
    // Torc
    void    GetSafe                 (TorcNetworkRequest* Request);
    void    CancelSafe              (TorcNetworkRequest* Request);
    void    PokeSafe                (TorcNetworkRequest* Request);
    void    GetAsynchronousSafe     (TorcNetworkRequest* Request, QObject *Parent);

  public:
    virtual ~TorcNetwork();

  protected:
    static void Setup               (bool Create);

  protected:
    TorcNetwork();
    bool    IsOnline                (void);
    bool    IsOwnAddressPriv        (const QHostAddress &Address);
    bool    CheckHeaders            (TorcNetworkRequest* Request, QNetworkReply *Reply);
    bool    Redirected              (TorcNetworkRequest* Request, QNetworkReply *Reply);

    void    CloseConnections        (void);
    void    UpdateConfiguration     (bool Creating = false);

  private:
    bool                             m_online;
    QNetworkConfigurationManager     m_manager;
    QNetworkConfiguration            m_configuration;
    QStringList                      m_hostNames;

    QMap<QNetworkReply*,TorcNetworkRequest*> m_requests;
    QMap<TorcNetworkRequest*,QNetworkReply*> m_reverseRequests;
    QMap<TorcNetworkRequest*,QObject*>       m_asynchronousRequests;
};

extern TorcNetwork *gNetwork;
extern QMutex      *gNetworkLock;

#endif // TORCNETWORK_H
