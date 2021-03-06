#ifndef TORCSSDP_H
#define TORCSSDP_H

// Qt
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QQueue>

// Torc

#include "torcqthread.h"
#include "torcupnp.h"
#include "torchttpserver.h"

#define TORC_SSDP_UDP_MULTICAST_PORT  1900
#define TORC_IPV4_UDP_MULTICAST_ADDR  QStringLiteral("239.255.255.250")
#define TORC_IPV4_UDP_MULTICAST_URL   QStringLiteral("%1:%2").arg(TORC_IPV4_UDP_MULTICAST_ADDR).arg(TORC_SSDP_UDP_MULTICAST_PORT)
#define TORC_IPV6_UDP_MULTICAST_ADDR  QStringLiteral("[FF05::C]")
#define TORC_IPV6_UDP_MULTICAST_ADDR2 QStringLiteral("FF05::C")
#define TORC_IPV6_UDP_MULTICAST_URL   QStringLiteral("%1:%2").arg(TORC_IPV6_UDP_MULTICAST_ADDR).arg(TORC_SSDP_UDP_MULTICAST_PORT)

class TorcSSDPSearchResponse
{
  public:
    enum ResponseType
    {
        None   = (0 << 0),
        Root   = (1 << 0),
        UUID   = (1 << 1),
        Device = (1 << 2)
    };

    Q_DECLARE_FLAGS(ResponseTypes, ResponseType)

    TorcSSDPSearchResponse(const QHostAddress &Address, const ResponseTypes Types, int Port);
    TorcSSDPSearchResponse(void);

  public:
    QHostAddress  m_responseAddress;
    ResponseTypes m_responseTypes;
    int           m_port;
};

class TorcSSDP final : public QObject
{
    friend class TorcSSDPThread;

    Q_OBJECT

  public:
    static void      Search             (TorcHTTPServer::Status Options);
    static void      CancelSearch       (void);
    static void      Announce           (TorcHTTPServer::Status Options);
    static void      CancelAnnounce     (void);

    static TorcSSDP                    *gSSDP;
    static QMutex                      *gSSDPLock;
    static bool                         gSearchEnabled;
    static TorcHTTPServer::Status       gSearchOptions;
    static bool                         gAnnounceEnabled;
    static TorcHTTPServer::Status       gAnnounceOptions;

  protected:
    TorcSSDP();
    ~TorcSSDP();

    static TorcSSDP* Create             (bool Destroy = false);

  protected slots:
    void             SearchPriv         (void);
    void             CancelSearchPriv   (void);
    void             AnnouncePriv       (void);
    void             CancelAnnouncePriv (void);

    void             SendSearch         (void);
    void             SendAnnounce       (bool IsIPv6, bool Alive);

    bool             event              (QEvent *Event) override;
    void             Read               (void);
    void             ProcessResponses   (void);

  private:
    Q_DISABLE_COPY(TorcSSDP)
    static QUdpSocket* CreateSearchSocket    (const QHostAddress &HostAddress, TorcSSDP *Owner);
    static QUdpSocket* CreateMulticastSocket (const QHostAddress &HostAddress, TorcSSDP *Owner, const QNetworkInterface &Interface);

    void             Start              (void);
    void             Stop               (void);
    void             Refresh            (void);
    void             ProcessDevice      (const QMap<QString,QString> &Headers, qint64 Expires, bool Add);
    qint64           GetExpiryTime      (const QString &Expires);
    void             StartSearch        (void);
    void             StopSearch         (void);
    void             StartAnnounce      (void);
    void             StopAnnounce       (void);
    void             ProcessResponse    (const TorcSSDPSearchResponse &Response);

  private:
    TorcHTTPServer::Status              m_searchOptions;
    TorcHTTPServer::Status              m_announceOptions;
    QString                             m_serverString;
    bool                                m_searching;
    bool                                m_searchDebugged;
    int                                 m_firstSearchTimer;
    int                                 m_secondSearchTimer;
    int                                 m_firstAnnounceTimer;
    int                                 m_secondAnnounceTimer;
    int                                 m_refreshTimer;
    bool                                m_started;
    QString                             m_ipv4Address;
    QString                             m_ipv6Address;
    QList<QHostAddress>                 m_addressess;
    QHostAddress                        m_ipv4GroupAddress;
    QUdpSocket                         *m_ipv4MulticastSocket;
    QHostAddress                        m_ipv6LinkGroupBaseAddress;
    QUdpSocket                         *m_ipv6LinkMulticastSocket;
    QHash<QString,TorcUPNPDescription>  m_discoveredDevices;
    QUdpSocket                         *m_ipv4UnicastSocket;
    QUdpSocket                         *m_ipv6UnicastSocket;
    QTimer                              m_responseTimer;
    QMap<qint64,TorcSSDPSearchResponse> m_responseQueue;
};

class TorcSSDPThread final : public TorcQThread
{
    Q_OBJECT

  public:
    TorcSSDPThread();

    void Start(void) override;
    void Finish(void) override;
};
#endif // TORCSSDP_H
