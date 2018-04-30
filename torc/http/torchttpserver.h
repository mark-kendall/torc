#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QThreadPool>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>
#include <QReadWriteLock>

// Torc
#include "torcsetting.h"
#include "torchtmlhandler.h"
#include "torchttpservices.h"
#include "torchtmlstaticcontent.h"
#include "torchtmldynamiccontent.h"
#include "torcupnpcontent.h"
#include "torcwebsocketpool.h"
#include "torcwebsocketthread.h"

class TorcHTTPConnection;
class TorcHTTPHandler;

class TorcHTTPServer : public QTcpServer
{
    Q_OBJECT

    friend class TorcHTTPServerObject;

  public:
    // Content/service handlers
    static void    Authorise          (TorcHTTPConnection *Connection, TorcHTTPRequest *Request, bool ForceCheck);
    static bool    AuthenticateUser   (TorcHTTPRequest *Request, QString &Username, bool &Stale);
    static void    ValidateOrigin     (TorcHTTPRequest *Request);
    static void    RegisterHandler    (TorcHTTPHandler *Handler);
    static void    DeregisterHandler  (TorcHTTPHandler *Handler);
    static void    HandleRequest      (TorcHTTPConnection *Connection, TorcHTTPRequest *Request);
    static QVariantMap HandleRequest  (const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated);
    static QVariantMap GetServiceHandlers (void);
    static QVariantMap GetServiceDescription(const QString &Service);

    // WebSockets
    static void    UpgradeSocket      (TorcHTTPRequest *Request, QTcpSocket *Socket);

    // server status
    static int     GetPort            (void);
    static bool    IsListening        (void);
    static QString PlatformName       (void);

  public:
    virtual       ~TorcHTTPServer     ();

  signals:
    void           HandlersChanged    (void);
    // WebSockets
    void           HandleUpgrade      (TorcHTTPRequest *Request, QTcpSocket *Socket);

  protected slots:

  protected:
    TorcHTTPServer ();
    void           incomingConnection (qintptr SocketDescriptor) Q_DECL_OVERRIDE Q_DECL_FINAL;
    bool           event              (QEvent *Event) Q_DECL_OVERRIDE Q_DECL_FINAL;
    bool           Open               (void);
    void           Close              (void);

  protected:
    static TorcHTTPServer*            gWebServer;
    static QMutex                     gWebServerLock;
    static QString                    gPlatform;
    static QString                    gOriginWhitelist;
    static QReadWriteLock             gOriginWhitelistLock;

  private:
    static void     UpdateOriginWhitelist (int Port);

  private:
    TorcSetting                      *m_port;
    TorcHTMLHandler                   m_defaultHandler;
    TorcHTTPServices                  m_servicesHandler;
    TorcHTMLStaticContent             m_staticContent;
    TorcHTMLDynamicContent            m_dynamicContent;
    TorcUPnPContent                   m_upnpContent;
    QThreadPool                       m_connectionPool;
    int                               m_abort;
    quint32                           m_httpBonjourReference;
    quint32                           m_torcBonjourReference;
    TorcWebSocketPool                 m_webSocketPool;
};

Q_DECLARE_METATYPE(TorcHTTPRequest*);
Q_DECLARE_METATYPE(QTcpSocket*);

#endif // TORCHTTPSERVER_H
