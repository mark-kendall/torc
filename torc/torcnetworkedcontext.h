#ifndef TORCNETWORKEDCONTEXT_H
#define TORCNETWORKEDCONTEXT_H

// Qt
#include <QObject>
#include <QReadWriteLock>
#include <QHostAddress>

// Torc
#include "torchttpservice.h"
#include "torclocalcontext.h"

class TorcRPCRequest;
class TorcNetworkRequest;
class TorcWebSocket;
class TorcWebSocketThread;
class TorcHTTPRequest;
class QTcpSocket;

class TorcNetworkService : public QObject
{
    Q_OBJECT
    Q_FLAGS(ServiceSource)

  public:
    enum ServiceSource
    {
        Spontaneous = (0 << 0),
        Bonjour     = (1 << 0),
        UPnP        = (1 << 1)
    };

    Q_DECLARE_FLAGS(ServiceSources, ServiceSource)

  public:
    static bool WeActAsServer (int Priority, qint64 StartTime, const QString &UUID);
    TorcNetworkService(const QString &Name, const QString &UUID, int Port, bool Secure, const QList<QHostAddress> &Addresses);
    ~TorcNetworkService();

    Q_PROPERTY (QString     name              READ GetName         CONSTANT)
    Q_PROPERTY (QString     uuid              READ GetUuid         CONSTANT)
    Q_PROPERTY (int         port              READ GetPort         CONSTANT)
    Q_PROPERTY (QString     uiAddress         READ GetAddress      CONSTANT)
    Q_PROPERTY (QString     host              READ GetHost         CONSTANT)
    Q_PROPERTY (qint64      startTime         READ GetStartTime    NOTIFY StartTimeChanged)
    Q_PROPERTY (int         priority          READ GetPriority     NOTIFY PriorityChanged)
    Q_PROPERTY (QString     apiVersion        READ GetAPIVersion   NOTIFY ApiVersionChanged)
    Q_PROPERTY (bool        connected         READ GetConnected    NOTIFY ConnectedChanged)

  signals:
    void                    StartTimeChanged  (void);
    void                    PriorityChanged   (void);
    void                    ApiVersionChanged (void);
    void                    ConnectedChanged  (void);
    void                    TryConnect        (void);

  public slots:
    QString                 GetName           (void);
    QString                 GetUuid           (void);
    int                     GetPort           (void);
    QString                 GetHost           (void);
    QList<QHostAddress>     GetAddresses      (void);
    QString                 GetAddress        (void);
    qint64                  GetStartTime      (void);
    int                     GetPriority       (void);
    QString                 GetAPIVersion     (void);
    bool                    GetConnected      (void);

    void                    Connect           (void);
    void                    Connected         (void);
    void                    Disconnected      (void);
    void                    RequestReady      (TorcNetworkRequest *Request);
    void                    RequestReady      (TorcRPCRequest     *Request);

  public:
    void                    SetHost           (const QString &Host);
    void                    SetStartTime      (qint64 StartTime);
    void                    SetPriority       (int    Priority);
    void                    SetAPIVersion     (const QString &Version);
    void                    SetSource         (ServiceSource Source);
    ServiceSources          GetSources        (void);
    void                    RemoveSource      (ServiceSource Source);
    void                    SetWebSocketThread(TorcWebSocketThread *Thread);
    void                    RemoteRequest     (TorcRPCRequest *Request);
    void                    CancelRequest     (TorcRPCRequest *Request);
    QVariant                ToMap             (void);

  private:
    Q_DISABLE_COPY(TorcNetworkService)
    void                    ScheduleRetry     (void);
    void                    QueryPeerDetails  (void);

  private:
    QString                 name;
    QString                 uuid;
    int                     port;
    QString                 host;
    bool                    secure;
    QString                 uiAddress;
    qint64                  startTime;
    int                     priority;
    QString                 apiVersion;
    bool                    connected;

    ServiceSources          m_sources;
    QString                 m_debugString;
    QList<QHostAddress>     m_addresses;
    int                     m_preferredAddressIndex;
    int                     m_abort;
    TorcRPCRequest         *m_getPeerDetailsRPC;
    TorcNetworkRequest     *m_getPeerDetails;
    TorcWebSocketThread    *m_webSocketThread;
    bool                    m_retryScheduled;
    int                     m_retryInterval;
};

Q_DECLARE_METATYPE(TorcNetworkService*)
Q_DECLARE_OPERATORS_FOR_FLAGS(TorcNetworkService::ServiceSources)

class TorcNetworkedContext: public QObject, public TorcHTTPService
{
    friend class TorcNetworkedContextObject;
    friend class TorcNetworkService;

    Q_OBJECT
    Q_CLASSINFO("Version",    "1.0.0")
    Q_CLASSINFO("GetPeers",   "type=peers")
    Q_PROPERTY(QVariantList peers READ GetPeers NOTIFY PeersChanged)

  public:
    static void                PeerConnected       (TorcWebSocketThread* Thread, const QVariantMap &Data);
    static void                RemoteRequest       (const QString &UUID, TorcRPCRequest *Request);
    static void                CancelRequest       (const QString &UUID, TorcRPCRequest *Request, int Wait = 1000);

    // TorcHTTPService
    QString                    GetUIName           (void);

  signals:
    void                       PeersChanged        (void);
    void                       PeerConnected       (QString &Name, QString &UUID);
    void                       PeerDisconnected    (QString &Name, QString &UUID);
    void                       NewPeer             (TorcWebSocketThread* Socket, const QVariantMap &Data);
    void                       NewRequest          (const QString &UUID, TorcRPCRequest *Request);
    void                       RequestCancelled    (const QString &UUID, TorcRPCRequest *Request);

  public slots:
    QVariantList               GetPeers            (void);
    void                       SubscriberDeleted   (QObject *Subscriber);

  protected slots:
    void                       HandleNewPeer       (TorcWebSocketThread *Thread, const QVariantMap &Data);
    void                       HandleNewRequest    (const QString &UUID, TorcRPCRequest *Request);
    void                       HandleCancelRequest (const QString &UUID, TorcRPCRequest *Request);

  protected:
    TorcNetworkedContext();
    ~TorcNetworkedContext();

    void                       Connected           (TorcNetworkService* Peer);
    void                       Disconnected        (TorcNetworkService* Peer);
    bool                       event               (QEvent* Event);

  private:
    Q_DISABLE_COPY(TorcNetworkedContext)
    void                       Add                 (TorcNetworkService* Peer);
    void                       Remove              (const QString &UUID, TorcNetworkService::ServiceSource Source = TorcNetworkService::Spontaneous);

  private:
    QList<TorcNetworkService*> m_discoveredServices;
    QList<QString>             m_serviceList;
    QVariantList               peers; // dummy
};

extern TorcNetworkedContext *gNetworkedContext;

#endif // TORCNETWORKEDCONTEXT_H
