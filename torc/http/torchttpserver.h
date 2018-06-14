#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QTcpSocket>
#include <QMutex>
#include <QReadWriteLock>

// Torc
#include "torcuser.h"
#include "torchtmlhandler.h"
#include "torchttpservices.h"
#include "torchtmlstaticcontent.h"
#include "torchtmldynamiccontent.h"
#include "torcupnpcontent.h"
#include "torcwebsocketpool.h"
#include "torcwebsocketthread.h"
#include "torchttpserverlistener.h"

class TorcSetting;
class TorcHTTPHandler;
class TorcSSDPThread;

class TorcHTTPServer Q_DECL_FINAL : public QObject
{
    Q_OBJECT

    friend class TorcHTTPServerObject;

  public:
    class Status
    {
      public:
        Status();
        bool operator==(const Status &Other) const;
        int  port;
        bool secure;
        bool ipv4;
        bool ipv6;
    };

  public:
    // Content/service handlers
    static QString ServerDescription  (void);
    static Status  GetStatus          (void);
    static void    Authorise          (const QString &Host, TorcHTTPRequest &Request, bool ForceCheck);
    static void    AuthenticateUser   (TorcHTTPRequest &Request);
    static void    AddAuthenticationHeader(TorcHTTPRequest &Request);
    static void    ValidateOrigin     (TorcHTTPRequest &Request);
    static void    RegisterHandler    (TorcHTTPHandler *Handler);
    static void    DeregisterHandler  (TorcHTTPHandler *Handler);
    static void    HandleRequest      (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort,  TorcHTTPRequest &Request);
    static QVariantMap HandleRequest  (const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated);
    static QVariantMap GetServiceHandlers (void);
    static QVariantMap GetServiceDescription(const QString &Service);
    static TorcWebSocketThread* TakeSocket (TorcWebSocketThread *Socket);
    static QString PlatformName       (void);

  public:
    virtual       ~TorcHTTPServer     ();

  public slots:
    void           NewConnection      (qintptr SocketDescriptor);

  protected slots:
    void           PortChanged        (int Port);
    void           SecureChanged      (bool Secure);
    void           UPnPChanged        (bool UPnP);
    void           UPnPSearchChanged  (bool Search);
    void           UPnPAdvertChanged  (bool Advert);
    void           BonjourChanged     (bool Bonjour);
    void           BonjourSearchChanged(bool Search);
    void           BonjourAdvertChanged(bool Advert);
    void           IPv6Changed        (bool IPv6);
    void           Restart            (void);

  signals:
    void           HandlersChanged    (void);

  protected:
    TorcHTTPServer ();
    bool           event              (QEvent *Event) Q_DECL_OVERRIDE Q_DECL_FINAL;
    bool           Open               (void);
    void           Close              (void);
    TorcWebSocketThread* TakeSocketPriv(TorcWebSocketThread *Socket);

  protected:
    static TorcHTTPServer*            gWebServer;
    static Status                     gWebServerStatus;
    static QMutex                     gWebServerLock;
    static QString                    gPlatform;
    static QString                    gOriginWhitelist;
    static QReadWriteLock             gOriginWhitelistLock;

  private:
    static void    UpdateOriginWhitelist (TorcHTTPServer::Status Status);
    void           StartBonjour       (void);
    void           StopBonjour        (void);
    void           StopBonjourBrowse  (void);
    void           StopBonjourAdvert  (void);
    void           StartUPnP          (void);
    void           StopUPnP           (void);

  private:
    TorcSettingGroup                 *m_serverSettings;
    TorcSetting                      *m_port;
    TorcSetting                      *m_secure;
    TorcSetting                      *m_upnp;
    TorcSetting                      *m_upnpSearch;
    TorcSetting                      *m_upnpAdvertise;
    TorcSetting                      *m_bonjour;
    TorcSetting                      *m_bonjourSearch;
    TorcSetting                      *m_bonjourAdvert;
    TorcSetting                      *m_ipv6;
    TorcHTTPServerListener           *m_listener;
    TorcUser                          m_user;
    TorcHTMLHandler                   m_defaultHandler;
    TorcHTTPServices                  m_servicesHandler;
    TorcHTMLStaticContent             m_staticContent;
    TorcHTMLDynamicContent            m_dynamicContent;
    TorcUPnPContent                   m_upnpContent;
    TorcSSDPThread                   *m_ssdpThread;
    quint32                           m_bonjourBrowserReference;
    quint32                           m_httpBonjourReference;
    quint32                           m_torcBonjourReference;
    TorcWebSocketPool                 m_webSocketPool;

  private:
    Q_DISABLE_COPY(TorcHTTPServer)
};

Q_DECLARE_METATYPE(TorcHTTPRequest*)
Q_DECLARE_METATYPE(QTcpSocket*)
Q_DECLARE_METATYPE(QHostAddress)
#endif // TORCHTTPSERVER_H
