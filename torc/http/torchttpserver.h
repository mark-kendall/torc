#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QThreadPool>
#include <QTcpServer>
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

class TorcSetting;
class TorcHTTPHandler;

class TorcHTTPServer : public QTcpServer
{
    Q_OBJECT

    friend class TorcHTTPServerObject;

  public:
    // Content/service handlers
    static QString ServerDescription  (void);
    static bool    IsSecure           (void);
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

    // server status
    static int     GetPort            (void);
    static bool    IsListening        (void);
    static QString PlatformName       (void);

  public:
    virtual       ~TorcHTTPServer     ();

  public slots:
    void           PortChanged        (int Port);
    void           SecureChanged      (bool Secure);
    void           Restart            (void);

  signals:
    void           HandlersChanged    (void);

  protected:
    TorcHTTPServer ();
    void           incomingConnection (qintptr SocketDescriptor) Q_DECL_OVERRIDE Q_DECL_FINAL;
    bool           event              (QEvent *Event) Q_DECL_OVERRIDE Q_DECL_FINAL;
    bool           Open               (void);
    void           Close              (void);
    TorcWebSocketThread* TakeSocketPriv(TorcWebSocketThread *Socket);

  protected:
    static TorcHTTPServer*            gWebServer;
    static QMutex                     gWebServerLock;
    static QString                    gPlatform;
    static QString                    gOriginWhitelist;
    static QReadWriteLock             gOriginWhitelistLock;
    static bool                       gWebServerSecure;

  private:
    static void     UpdateOriginWhitelist (int Port);

  private:
    TorcSettingGroup                 *m_serverSettings;
    TorcSetting                      *m_port;
    TorcSetting                      *m_secure;
    TorcUser                          m_user;
    TorcHTMLHandler                   m_defaultHandler;
    TorcHTTPServices                  m_servicesHandler;
    TorcHTMLStaticContent             m_staticContent;
    TorcHTMLDynamicContent            m_dynamicContent;
    TorcUPnPContent                   m_upnpContent;
    quint32                           m_httpBonjourReference;
    quint32                           m_torcBonjourReference;
    TorcWebSocketPool                 m_webSocketPool;

  private:
    Q_DISABLE_COPY(TorcHTTPServer)
};

Q_DECLARE_METATYPE(TorcHTTPRequest*);
Q_DECLARE_METATYPE(QTcpSocket*);
Q_DECLARE_METATYPE(QHostAddress);

#endif // TORCHTTPSERVER_H
