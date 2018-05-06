/* Class TorcHTTPServer
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QUrl>
#include <QTcpSocket>
#include <QCoreApplication>

// Torc
#include "torccompat.h"
#include "torclocalcontext.h"
#include "torclanguage.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torchttprequest.h"
#include "torchtmlhandler.h"
#include "torchttphandler.h"
#include "torchttpservice.h"
#include "torchttpserver.h"
#include "torcbonjour.h"
#include "torcssdp.h"
#include "torcwebsockettoken.h"
#include "torchttpservernonce.h"

#ifndef Q_OS_WIN
#include <sys/utsname.h>
#endif

QMap<QString,TorcHTTPHandler*> gHandlers;
QReadWriteLock*                gHandlersLock = new QReadWriteLock(QReadWriteLock::Recursive);
QString                        gServicesDirectory(SERVICES_DIRECTORY);


void TorcHTTPServer::RegisterHandler(TorcHTTPHandler *Handler)
{
    if (!Handler)
        return;

    bool changed = false;

    {
        QWriteLocker locker(gHandlersLock);

        foreach (QString signature, Handler->Signature().split(","))
        {
            if (gHandlers.contains(signature))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Handler '%1' for '%2' already registered - ignoring").arg(Handler->Name()).arg(signature));
            }
            else if (!signature.isEmpty())
            {
                LOG(VB_GENERAL, LOG_DEBUG, QString("Added handler '%1' for %2").arg(Handler->Name()).arg(signature));
                gHandlers.insert(signature, Handler);
                changed = true;
            }
        }
    }

    if (changed && gWebServer)
        emit gWebServer->HandlersChanged();
}

void TorcHTTPServer::DeregisterHandler(TorcHTTPHandler *Handler)
{
    if (!Handler)
        return;

    bool changed = false;

    {
        QWriteLocker locker(gHandlersLock);

        foreach (QString signature, Handler->Signature().split(","))
        {
            QMap<QString,TorcHTTPHandler*>::iterator it = gHandlers.find(signature);
            if (it != gHandlers.end())
            {
                LOG(VB_GENERAL, LOG_DEBUG, QString("Removing handler '%1'").arg(it.key()));
                gHandlers.erase(it);
                changed = true;
            }
        }
    }

    if (changed && gWebServer)
        emit gWebServer->HandlersChanged();
}

void TorcHTTPServer::HandleRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort,  TorcHTTPRequest &Request)
{
    // this should already have been checked - but better safe than sorry
    if (!Request.IsAuthorised())
        return;

    // verify cross domain requests
    // should an invalid origin fail?
    TorcHTTPServer::ValidateOrigin(Request);

    QReadLocker locker(gHandlersLock);

    QString path = Request.GetPath();

    QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.find(path);
    if (it != gHandlers.end())
    {
        // direct path match
        (*it)->ProcessHTTPRequest(PeerAddress, PeerPort, LocalAddress, LocalPort, Request);
    }
    else
    {
        // fully recursive handler
        it = gHandlers.begin();
        for ( ; it != gHandlers.end(); ++it)
        {
            if ((*it)->GetRecursive() && path.startsWith(it.key()))
            {
                (*it)->ProcessHTTPRequest(PeerAddress, PeerPort, LocalAddress, LocalPort, Request);
                break;
            }
        }
    }
}

/*! \brief Process an incoming RPC request.
 *
 * Remote procedure calls are only allowed for 'service' directories and will be ignored
 * for anything else (e.g. attempts to retrieve static content and other files).
 *
 * \note The structure of responses is largely consistent with the JSON-RPC 2.0 specification. As such,
 * successful requests will have a 'result' entry and failed requests will have an 'error' field with
 * an error code and error message.
*/
QVariantMap TorcHTTPServer::HandleRequest(const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated)
{
    QReadLocker locker(gHandlersLock);

    QString path = "/";
    int index = Method.lastIndexOf("/");
    if (index > -1)
        path = Method.left(index + 1).trimmed();

    // ensure this is a services call
    if (path.startsWith(gServicesDirectory))
    {
        // NB no recursive path handling here
        QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.find(path);
        if (it != gHandlers.end())
            return (*it)->ProcessRequest(Method, Parameters, Connection, Authenticated);
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Method '%1' not found in services").arg(Method));

    QVariantMap result;
    QVariantMap error;
    error.insert("code", -32601);
    error.insert("message", "Method not found");
    result.insert("error", error);
    return result;
}

QVariantMap TorcHTTPServer::GetServiceHandlers(void)
{
    QReadLocker locker(gHandlersLock);

    QVariantMap result;

    QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.begin();
    for ( ; it != gHandlers.end(); ++it)
    {
        TorcHTTPService *service = dynamic_cast<TorcHTTPService*>(it.value());
        if (service)
        {
            QVariantMap map;
            map.insert("path", it.key());
            map.insert("name", service->GetUIName());
            result.insert(it.value()->Name(), map);
        }
    }
    return result;
}

QVariantMap TorcHTTPServer::GetServiceDescription(const QString &Service)
{
    QReadLocker locker(gHandlersLock);

    QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.begin();
    for ( ; it != gHandlers.end(); ++it)
    {
        if (it.value()->Name() == Service)
        {
            TorcHTTPService *service = dynamic_cast<TorcHTTPService*>(it.value());
            if (service)
                return service->GetServiceDetails();
        }
    }

    return QVariantMap();
}

int TorcHTTPServer::GetPort(void)
{
    QMutexLocker locker(&gWebServerLock);

    if (gWebServer)
        return gWebServer->serverPort();

    return 0;
}

bool TorcHTTPServer::IsListening(void)
{
    QMutexLocker locker(&gWebServerLock);

    if (gWebServer)
        return gWebServer->isListening();

    return false;
}

QString TorcHTTPServer::PlatformName(void)
{
    return gPlatform;
}

/*! \class TorcHTTPServer
 *  \brief An HTTP server.
 *
 * TorcHTTPServer listens for incoming TCP connections. The default port is 4840
 * though any available port may be used if the default is unavailable. New
 * connections are passed to to TorcWebSocketPool.
 *
 * Register new content handlers with RegisterHandler and remove
 * them with DeregisterHandler. These can then be used with the static
 * HandleRequest methods.
 *
 * \sa TorcHTTPRequest
 * \sa TorcHTTPHandler
 *
 * \todo Fix potential socket and request leak in UpgradeSocket.
 * \todo Add setting for authentication (and hence full username/password requirement).
 * \todo Add setting for SSL.
*/

TorcHTTPServer* TorcHTTPServer::gWebServer = NULL;
QMutex          TorcHTTPServer::gWebServerLock(QMutex::Recursive);
QString         TorcHTTPServer::gPlatform = QString("");
QString         TorcHTTPServer::gOriginWhitelist = QString("");
QReadWriteLock  TorcHTTPServer::gOriginWhitelistLock(QReadWriteLock::Recursive);

TorcHTTPServer::TorcHTTPServer()
  : QTcpServer(),
    m_port(NULL),
    m_defaultHandler("", QCoreApplication::applicationName()), // default top level handler
    m_servicesHandler(this),                                   // services 'helper' service for '/services'
    m_staticContent(),                                         // static files - for /css /fonts /js /img etc
    m_dynamicContent(),                                        // dynamic files - for config files etc (typically served from ~/.torc/content)
    m_upnpContent(),                                           // upnp - device description
    m_httpBonjourReference(0),
    m_torcBonjourReference(0),
    m_webSocketPool()
{
    // port setting - this could become a user editable setting
    m_port = new TorcSetting(NULL, TORC_CORE + "WebServerPort", QString(), TorcSetting::Integer, true, QVariant((int)4840));

    // initialise platform name
    static bool initialised = false;
    if (!initialised)
    {
        initialised = true;
        gPlatform = QString("%1, Version: %2 ").arg(QCoreApplication::applicationName()).arg(GIT_VERSION);
#ifdef Q_OS_WIN
        gPlatform += QString("(Windows %1.%2)").arg(LOBYTE(LOWORD(GetVersion()))).arg(HIBYTE(LOWORD(GetVersion())));
#else
        struct utsname info;
        uname(&info);
        gPlatform += QString("(%1 %2)").arg(info.sysname).arg(info.release);
#endif
    }

    // listen for host name updates
    gLocalContext->AddObserver(this);

    // and start
    Open();
}

TorcHTTPServer::~TorcHTTPServer()
{
    disconnect();

    gLocalContext->RemoveObserver(this);

    Close();

    if (m_port)
    {
        m_port->Remove();
        m_port->DownRef();
        m_port = NULL;
    }
}

TorcWebSocketThread* TorcHTTPServer::TakeSocket(TorcWebSocketThread *Socket)
{
    QMutexLocker locker(&gWebServerLock);
    if (gWebServer)
        return gWebServer->TakeSocketPriv(Socket);
    return NULL;
}

/*! \brief Ensures remote user is authorised to access this request.
 *
 * The 'Authorization' header is used for standard HTTP requests.
 *
 * Authentication credentials supplied via the url are used for WebSocket upgrade requests - and it is assumed the if the
 * initial request is authenticated, the entire socket is then authenticated for that user.
 *
 * \todo Use proper username and password.
*/
void TorcHTTPServer::Authorise(const QString &Host, TorcHTTPRequest &Request, bool ForceCheck)
{
    // We allow unauthenticated access to anything that doesn't alter state.
    // N.B. If a rogue peer posts to a 'setter' type function using a GET type request, the request
    // will fail during processing - so don't validate the method here as it may lead to fall positives.
    // ForceCheck is used to process any authentication headers for WebSocket upgrade requests.
    HTTPRequestType type = Request.GetHTTPRequestType();
    bool authenticate = type == HTTPPost || type == HTTPPut || type == HTTPDelete || type == HTTPUnknownType;

    if (!authenticate && !ForceCheck)
    {
        Request.Authorise(HTTPAuthorised);
        return;
    }

    // authentication token supplied in the url (WebSocket)
    // do this before 'standard' authentication to ensure token is checked and expired
    if (Request.Queries().contains("accesstoken"))
    {
        QString token = Request.Queries().value("accesstoken");
        if (token == TorcWebSocketToken::GetWebSocketToken(Host, token))
        {
            Request.Authorise(HTTPAuthorised);
            return;
        }
    }

    // explicit authorization header
    // N.B. This will also authorise websocket clients who send authorisation headers with
    // the upgrade request i.e. there is no need to use the accesstoken route IF the correct headers
    // are sent. Use GetWebSocketToken (which is a PUT operation and requires authorisation) to force
    // clients to authenticate (i.e. browsers).
    TorcHTTPServer::AuthenticateUser(Request);
    if (Request.IsAuthorised() == HTTPAuthorised)
        return;

    Request.SetResponseType(HTTPResponseNone);
    Request.SetStatus(HTTP_Unauthorized);

    // HTTP 1.1 should support Digest
    if (Request.GetHTTPProtocol() > HTTPOneDotZero)
    {
        TorcHTTPServerNonce::ProcessDigestAuth(Request);
    }
    else // otherwise fallback to Basic
    {
        Request.SetResponseHeader("WWW-Authenticate", QString("Basic realm=\"%1\"").arg(TORC_REALM));
    }
}

/*! \brief Check the Origin header for validity and respond appropriately.
 *
 * Cross domain requests are not common from well behaved clients/browsers. There can however be confusion
 * when the IP address is a localhost (IPv4 or IPv6) and we can never guarantee that the domain will 'look' equivalent.
 * So validate the incoming origin (which is good practice anyway) and set the outgoing headers as necessary.
*/
void TorcHTTPServer::ValidateOrigin(TorcHTTPRequest &Request)
{
    gOriginWhitelistLock.lockForRead();
    if (Request.Headers()->contains("Origin") && gOriginWhitelist.contains(Request.Headers()->value("Origin"), Qt::CaseInsensitive))
    {
        Request.SetResponseHeader("Access-Control-Allow-Origin", Request.Headers()->value("Origin"));
        Request.SetResponseHeader("Access-Control-Allow-Credentials", "true");
    }
    gOriginWhitelistLock.unlock();
}

void TorcHTTPServer::AuthenticateUser(TorcHTTPRequest &Request)
{
    if (!Request.Headers()->contains("Authorization"))
        return;

    QString header = Request.Headers()->value("Authorization");
    static QString username("admin");
    static QString password("1234");

    // most clients will support Digest Access Authentication, so try that first
    if (header.startsWith("Digest", Qt::CaseInsensitive))
    {
        TorcHTTPServerNonce::ProcessDigestAuth(Request, username, password);
    }
    else if (header.startsWith("Basic", Qt::CaseInsensitive))
    {
        /* enable this once Digest authentication is enabled for Torc to Torc websocket connections (TorcNetworkedContext)

        // only accept Basic authentication if using < HTTP 1.1
        if (Request.GetHTTPProtocol() > HTTPOneDotZero)
        {
            LOG(VB_GENERAL, LOG_WARNING, "Disallowing basic authentication for HTTP 1.1 client");
        }
        else */
        {
            // remove leading 'Basic' and split off token
            QString authentication = header.mid(5).trimmed();
            QStringList userinfo   = QString(QByteArray::fromBase64(authentication.toUtf8())).split(':');
            if ((userinfo.size() == 2) && (userinfo[0] == username) && (userinfo[1] == password))
                Request.Authorise(HTTPAuthorised);
        }
    }
}

/*! \brief Create the 'Origin' whitelist for cross domain requests
 *
 * \note This assumes the server is listening on all interfaces (currently true).
*/
void TorcHTTPServer::UpdateOriginWhitelist(int Port)
{
    QWriteLocker locker(&gOriginWhitelistLock);

    // localhost first
    gOriginWhitelist = "http://localhost:" + QString::number(Port) + " ";

    // all known raw IP addresses
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (int i = 0; i < addresses.size(); ++i)
        gOriginWhitelist += QString("%1%2 ").arg("http://").arg(TorcNetwork::IPAddressToLiteral(addresses[i], Port, false));

    // and any known host names
    QStringList hosts = TorcNetwork::GetHostNames();
    foreach (QString host, hosts)
        if (!host.isEmpty())
            gOriginWhitelist += QString("%1%2:%3 ").arg("http://").arg(host).arg(Port);

    LOG(VB_NETWORK, LOG_INFO, "Origin whitelist: " + gOriginWhitelist);
}

bool TorcHTTPServer::Open(void)
{
    int port = m_port->GetValue().toInt();
    bool waslistening = isListening();

    if (!waslistening)
    {
        if (!listen(QHostAddress::Any, port))
            if (port > 0)
                listen();
    }

    if (!isListening())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open web server port");
        Close();
        return false;
    }

    // try to use the same port
    if (port != serverPort())
    {
        port = serverPort();
        m_port->SetValue(QVariant((int)port));

        // re-advertise if the port has changed
        if (m_httpBonjourReference)
        {
            TorcBonjour::Instance()->Deregister(m_httpBonjourReference);
            m_httpBonjourReference = 0;
        }

        if (m_torcBonjourReference)
        {
            TorcBonjour::Instance()->Deregister(m_torcBonjourReference);
            m_torcBonjourReference = 0;
        }
    }

    // advertise service if not already doing so
    if (!m_httpBonjourReference || !m_torcBonjourReference)
    {
        // add the 'root' apiversion, as would be returned by '/services/GetServiceVersion'
        int index = TorcHTTPServices::staticMetaObject.indexOfClassInfo("Version");

        QMap<QByteArray,QByteArray> map;
        map.insert("uuid", gLocalContext->GetUuid().toLatin1());
        map.insert("apiversion", (index > -1) ? TorcHTTPServices::staticMetaObject.classInfo(index).value() : "unknown");
        map.insert("priority",   QByteArray::number(gLocalContext->GetPriority()));
        map.insert("starttime",  QByteArray::number(gLocalContext->GetStartTime()));

        QByteArray name(QCoreApplication::applicationName().toLatin1());
        name.append(" on ");
        name.append(QHostInfo::localHostName());

        if (!m_httpBonjourReference)
            m_httpBonjourReference = TorcBonjour::Instance()->Register(port, "_http._tcp.", name, map);

        if (!m_torcBonjourReference)
            m_torcBonjourReference = TorcBonjour::Instance()->Register(port, "_torc._tcp", name, map);
    }

    TorcSSDP::Announce();

    if (!waslistening)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Web server listening on port %1").arg(port));
        UpdateOriginWhitelist(m_port->GetValue().toInt());
    }

    return true;
}

void TorcHTTPServer::Close(void)
{
    // stop advertising
    if (m_httpBonjourReference)
    {
        TorcBonjour::Instance()->Deregister(m_httpBonjourReference);
        m_httpBonjourReference = 0;
    }

    if (m_torcBonjourReference)
    {
        TorcBonjour::Instance()->Deregister(m_torcBonjourReference);
        m_torcBonjourReference = 0;
    }

    TorcSSDP::CancelAnnounce();

    // close connections
    m_webSocketPool.CloseSockets();

    // actually close
    close();

    LOG(VB_GENERAL, LOG_INFO, "Webserver closed");
}

bool TorcHTTPServer::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            switch (torcevent->GetEvent())
            {
                // these events all notify that the list of interfaces and addresses the server is
                // listening on will have been updated
                case Torc::NetworkAvailable:
                case Torc::NetworkUnavailable:
                case Torc::NetworkChanged:
                case Torc::NetworkHostNamesChanged:
                    UpdateOriginWhitelist(m_port->GetValue().toInt());
                    break;
                default:
                    break;
            }
        }
    }

    return QTcpServer::event(Event);
}

TorcWebSocketThread* TorcHTTPServer::TakeSocketPriv(TorcWebSocketThread *Socket)
{
    return m_webSocketPool.TakeSocket(Socket);
}

void TorcHTTPServer::incomingConnection(qintptr SocketDescriptor)
{
    m_webSocketPool.IncomingConnection(SocketDescriptor);
}

class TorcHTTPServerObject : public TorcAdminObject, public TorcStringFactory
{
  public:
    TorcHTTPServerObject()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY)
    {
        qRegisterMetaType<TorcHTTPRequest*>();
        qRegisterMetaType<TorcHTTPService*>();
        qRegisterMetaType<QTcpSocket*>();
        qRegisterMetaType<QHostAddress>();
    }

    void GetStrings(QVariantMap &Strings)
    {
        Strings.insert("ServerApplication",       QCoreApplication::applicationName());

        Strings.insert("SocketNotConnected",      QCoreApplication::translate("TorcHTTPServer", "Not connected"));
        Strings.insert("SocketConnecting",        QCoreApplication::translate("TorcHTTPServer", "Connecting"));
        Strings.insert("SocketConnected",         QCoreApplication::translate("TorcHTTPServer", "Connected"));
        Strings.insert("SocketReady",             QCoreApplication::translate("TorcHTTPServer", "Ready"));
        Strings.insert("SocketDisconnecting",     QCoreApplication::translate("TorcHTTPServer", "Disconnecting"));
        Strings.insert("ConnectedTo",             QCoreApplication::translate("TorcHTTPServer", "Connected to"));
        Strings.insert("ConnectTo",               QCoreApplication::translate("TorcHTTPServer", "Connect to"));

        Strings.insert("SocketReconnectAfterMs",  10000); // try and reconnect every 10 seconds
    }

    void Create(void)
    {
        Destroy();

        QMutexLocker locker(&TorcHTTPServer::gWebServerLock);
        TorcHTTPServer::gWebServer = new TorcHTTPServer();
    }

    void Destroy(void)
    {
        QMutexLocker locker(&TorcHTTPServer::gWebServerLock);

        // this may need to use deleteLater but this causes issues with setting deletion
        // as the object is actually destroyed after the parent (network) setting
        if (TorcHTTPServer::gWebServer)
            delete TorcHTTPServer::gWebServer;

        TorcHTTPServer::gWebServer = NULL;
    }
} TorcHTTPServerObject;


