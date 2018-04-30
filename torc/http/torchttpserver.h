#ifndef TORCHTTPSERVER_H
#define TORCHTTPSERVER_H

// Qt
#include <QThreadPool>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMutex>

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
    QString        GetWebSocketToken  (TorcHTTPConnection *Connection, TorcHTTPRequest *Request);
    void           Authorise          (TorcHTTPConnection *Connection, TorcHTTPRequest *Request, bool ForceCheck);
    void           ValidateOrigin     (TorcHTTPRequest *Request);

  signals:
    void           HandlersChanged    (void);
    // WebSockets
    void           HandleUpgrade      (TorcHTTPRequest *Request, QTcpSocket *Socket);

  protected slots:

  protected:
    TorcHTTPServer ();
    void           incomingConnection (qintptr SocketDescriptor);
    bool           event              (QEvent *Event);
    bool           Open               (void);
    void           Close              (void);

  protected:
    static TorcHTTPServer*            gWebServer;
    static QMutex*                    gWebServerLock;
    static QString                    gPlatform;

  private:
    static void    ExpireWebSocketTokens (void);
    bool           AuthenticateUser      (TorcHTTPRequest *Request, QString &Username, bool &Stale);
    void           UpdateOriginWhitelist (void);

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

    QString                           m_originWhitelist;
};

Q_DECLARE_METATYPE(TorcHTTPRequest*);
Q_DECLARE_METATYPE(QTcpSocket*);

#endif // TORCHTTPSERVER_H
