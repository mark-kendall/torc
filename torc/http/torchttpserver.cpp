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
#include "torcsetting.h"
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

QMap<QString,TorcHTTPHandler*> gHandlers;
QReadWriteLock*                gHandlersLock = new QReadWriteLock(QReadWriteLock::Recursive);
QString                        gServicesDirectory(TORC_SERVICES_DIR);

TorcHTTPServer::Status::Status()
  : port(0),
    secure(false),
    ipv4(true),
    ipv6(true)
{
}

bool TorcHTTPServer::Status::operator ==(Status Other) const
{
    return port   == Other.port &&
           secure == Other.secure &&
           ipv4   == Other.ipv4 &&
           ipv6   == Other.ipv6;
}

void TorcHTTPServer::RegisterHandler(TorcHTTPHandler *Handler)
{
    bool changed = false;

    {
        QWriteLocker locker(gHandlersLock);

        if (!Handler)
            return;

        foreach (const QString &signature, Handler->Signature().split(","))
        {
            if (gHandlers.contains(signature))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Handler '%1' for '%2' already registered - ignoring").arg(Handler->Name(), signature));
            }
            else if (!signature.isEmpty())
            {
                LOG(VB_GENERAL, LOG_DEBUG, QString("Added handler '%1' for %2").arg(Handler->Name(), signature));
                gHandlers.insert(signature, Handler);
                changed = true;
            }
        }
    }

    {
        QMutexLocker locker(&gWebServerLock);
        if (changed && gWebServer)
            emit gWebServer->HandlersChanged();
    }
}

void TorcHTTPServer::DeregisterHandler(TorcHTTPHandler *Handler)
{
    bool changed = false;

    {
        QWriteLocker locker(gHandlersLock);

        if (!Handler)
            return;

        foreach (const QString &signature, Handler->Signature().split(","))
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

    {
        QMutexLocker locker(&gWebServerLock);
        if (changed && gWebServer)
            emit gWebServer->HandlersChanged();
    }
}

void TorcHTTPServer::HandleRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort,  TorcHTTPRequest &Request)
{
    // this should already have been checked - but better safe than sorry
    if (!Request.IsAuthorised())
        return;

    {
        QReadLocker locker(gHandlersLock);

        QString path = Request.GetPath();

        QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.constFind(path);
        if (it != gHandlers.constEnd())
        {
            // direct path match
            (*it)->ProcessHTTPRequest(PeerAddress, PeerPort, LocalAddress, LocalPort, Request);
        }
        else
        {
            // fully recursive handler
            it = gHandlers.constBegin();
            for ( ; it != gHandlers.constEnd(); ++it)
            {
                if ((*it)->GetRecursive() && path.startsWith(it.key()))
                {
                    (*it)->ProcessHTTPRequest(PeerAddress, PeerPort, LocalAddress, LocalPort, Request);
                    break;
                }
            }
        }
    }

    // verify cross domain requests
    TorcHTTPServer::ValidateOrigin(Request);
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
        QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.constFind(path);
        if (it != gHandlers.constEnd())
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

    QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.constBegin();
    for ( ; it != gHandlers.constEnd(); ++it)
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

    QMap<QString,TorcHTTPHandler*>::const_iterator it = gHandlers.constBegin();
    for ( ; it != gHandlers.constEnd(); ++it)
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

TorcHTTPServer::Status TorcHTTPServer::GetStatus(void)
{
    QMutexLocker locker(&gWebServerLock);
    Status result = gWebServerStatus;
    return result;
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
 * connections are passed to TorcWebSocketPool.
 *
 * Register new content handlers with RegisterHandler and remove
 * them with DeregisterHandler. These can then be used with the static
 * HandleRequest methods.
 *
 * \sa TorcHTTPRequest
 * \sa TorcHTTPHandler
*/

TorcHTTPServer* TorcHTTPServer::gWebServer = nullptr;
TorcHTTPServer::Status TorcHTTPServer::gWebServerStatus = TorcHTTPServer::Status();
QMutex          TorcHTTPServer::gWebServerLock(QMutex::Recursive);
QString         TorcHTTPServer::gPlatform = QString("");
QString         TorcHTTPServer::gOriginWhitelist = QString("");
QReadWriteLock  TorcHTTPServer::gOriginWhitelistLock(QReadWriteLock::Recursive);

TorcHTTPServer::TorcHTTPServer()
  : QObject(),
    m_serverSettings(nullptr),
    m_port(nullptr),
    m_secure(nullptr),
    m_upnp(nullptr),
    m_upnpSearch(nullptr),
    m_upnpAdvertise(nullptr),
    m_bonjour(nullptr),
    m_bonjourSearch(nullptr),
    m_bonjourAdvert(nullptr),
    m_ipv6(nullptr),
    m_listener(nullptr),
    m_user(),
    m_defaultHandler("", TORC_TORC), // default top level handler
    m_servicesHandler(this),         // services 'helper' service for '/services'
    m_staticContent(),               // static files - for /css /fonts /js /img etc
    m_dynamicContent(),              // dynamic files - for config files etc (typically served from ~/.torc/content)
    m_upnpContent(),                 // upnp - device description
    m_ssdpThread(nullptr),
    m_bonjourBrowserReference(0),
    m_httpBonjourReference(0),
    m_torcBonjourReference(0),
    m_webSocketPool()
{
    // if app is running with root privilges (e.g. Raspberry Pi) then try and default to sensible port settings
    // when first run. No point in trying 443 for secure sockets as SSL is not enabled by default (would require
    // additional setup step).
    m_serverSettings = new TorcSettingGroup(gRootSetting, tr("Server"));
    bool root = !geteuid();
    m_port   = new TorcSetting(m_serverSettings, TORC_PORT_SERVICE, tr("Port"), TorcSetting::Integer,
                               TorcSetting::Persistent | TorcSetting::Public, QVariant((int)(root ? 80 : 4840)));
    m_port->SetRange(root ? 1 : 1024, 65535, 1);
    m_port->SetActive(true);
    connect(m_port, QOverload<int>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::PortChanged);
    m_port->SetHelpText(tr("The port the server will listen on for incoming connections"));

    m_secure = new TorcSetting(m_serverSettings, TORC_SSL_SERVICE, tr("Secure sockets"), TorcSetting::Bool,
                               TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)false));
    m_secure->SetHelpText(tr("Use encrypted (SSL/TLS) connections to the server"));
    m_secure->SetActive(true);
    connect(m_secure, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::SecureChanged);

    m_upnp =  new TorcSetting(m_serverSettings, "ServerUPnP", tr("UPnP"), TorcSetting::Bool,
                              TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_upnp->SetActive(true);
    connect(m_upnp, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::UPnPChanged);

    m_upnpSearch = new TorcSetting(m_upnp, "ServerUPnPSearch", tr("UPnP Search"), TorcSetting::Bool,
                                   TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_upnpSearch->SetHelpText(tr("Use UPnP to search for other devices"));
    m_upnpSearch->SetActive(m_upnp->GetValue().toBool());
    connect(m_upnpSearch, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::UPnPSearchChanged);
    connect(m_upnp,       QOverload<bool>::of(&TorcSetting::ValueChanged), m_upnpSearch, &TorcSetting::SetActive);

    m_upnpAdvertise = new TorcSetting(m_upnp, "ServerUPnpAdvert", tr("UPnP Advertisement"), TorcSetting::Bool,
                                      TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_upnpAdvertise->SetHelpText(tr("Use UPnP to advertise this device"));
    m_upnpAdvertise->SetActive(m_upnp->GetValue().toBool());
    connect(m_upnpAdvertise, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::UPnPAdvertChanged);
    connect(m_upnp,          QOverload<bool>::of(&TorcSetting::ValueChanged), m_upnpAdvertise, &TorcSetting::SetActive);

    m_ipv6 = new TorcSetting(m_serverSettings, "ServerIPv6", tr("IPv6"), TorcSetting::Bool,
                             TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_ipv6->SetHelpText(tr("Enable IPv6"));
    m_ipv6->SetActive(true);
    connect(m_ipv6, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::IPv6Changed);

    m_bonjour = new TorcSetting(m_ipv6, "ServerBonjour", tr("Bonjour"), TorcSetting::Bool,
                                TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_bonjour->SetActive(m_ipv6->GetValue().toBool());
    connect(m_bonjour, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::BonjourChanged);
    connect(m_ipv6,    QOverload<bool>::of(&TorcSetting::ValueChanged), m_bonjour, &TorcSetting::SetActive);

    m_bonjourSearch = new TorcSetting(m_bonjour, "ServerBonjourSearch", tr("Bonjour search"), TorcSetting::Bool,
                                      TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_bonjourSearch->SetHelpText(tr("Use Bonjour to search for other devices"));
    m_bonjourSearch->SetActiveThreshold(2);
    m_bonjourSearch->SetActive(m_bonjour->GetValue().toBool());
    m_bonjourSearch->SetActive(m_ipv6->GetValue().toBool());
    connect(m_bonjourSearch, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::BonjourSearchChanged);
    connect(m_bonjour,       QOverload<bool>::of(&TorcSetting::ValueChanged), m_bonjourSearch, &TorcSetting::SetActive);
    connect(m_ipv6,          QOverload<bool>::of(&TorcSetting::ValueChanged), m_bonjourSearch, &TorcSetting::SetActive);

    m_bonjourAdvert = new TorcSetting(m_bonjour, "ServerBonjourAdvert", tr("Bonjour Advertisement"), TorcSetting::Bool,
                                      TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true));
    m_bonjourAdvert->SetHelpText(tr("Use Bonjour to advertise this device"));
    m_bonjourAdvert->SetActiveThreshold(2);
    m_bonjourAdvert->SetActive(m_bonjour->GetValue().toBool());
    m_bonjourAdvert->SetActive(m_ipv6->GetValue().toBool());
    connect(m_bonjourAdvert, QOverload<bool>::of(&TorcSetting::ValueChanged), this, &TorcHTTPServer::BonjourAdvertChanged);
    connect(m_bonjour,       QOverload<bool>::of(&TorcSetting::ValueChanged), m_bonjourAdvert, &TorcSetting::SetActive);
    connect(m_ipv6,          QOverload<bool>::of(&TorcSetting::ValueChanged), m_bonjourAdvert, &TorcSetting::SetActive);

    // initialise external status
    {
        QMutexLocker locker(&gWebServerLock);
        gWebServerStatus.secure = m_secure->GetValue().toBool();
        gWebServerStatus.port   = m_port->GetValue().toInt();
        gWebServerStatus.ipv6   = m_ipv6->GetValue().toBool();
    }

    // initialise platform name
    static bool initialised = false;
    if (!initialised)
    {
        initialised = true;
        gPlatform = QString("%1, Version: %2 ").arg(TORC_TORC, GIT_VERSION);
#ifdef Q_OS_WIN
        gPlatform += QString("(Windows %1.%2)").arg(LOBYTE(LOWORD(GetVersion()))).arg(HIBYTE(LOWORD(GetVersion())));
#else
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
        gPlatform += QString("(%1 %2)").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
#else
        gPlatform += QString("(OldTorc OldVersion)");
#endif
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

    if (m_bonjourAdvert)
    {
        m_bonjourAdvert->Remove();
        m_bonjourAdvert->DownRef();
        m_bonjourAdvert = nullptr;
    }

    if (m_bonjourSearch)
    {
        m_bonjourSearch->Remove();
        m_bonjourSearch->DownRef();
        m_bonjourSearch = nullptr;
    }

    if (m_bonjour)
    {
        m_bonjour->Remove();
        m_bonjour->DownRef();
        m_bonjour = nullptr;
    }

    if (m_ipv6)
    {
        m_ipv6->Remove();
        m_ipv6->DownRef();
        m_ipv6 = nullptr;
    }

    if (m_upnpAdvertise)
    {
        m_upnpAdvertise->Remove();
        m_upnpAdvertise->DownRef();
        m_upnpAdvertise = nullptr;
    }

    if (m_upnpSearch)
    {
        m_upnpSearch->Remove();
        m_upnpSearch->DownRef();
        m_upnpSearch = nullptr;
    }

    if (m_upnp)
    {
        m_upnp->Remove();
        m_upnp->DownRef();
        m_upnp = nullptr;
    }

    if (m_port)
    {
        m_port->Remove();
        m_port->DownRef();
        m_port = nullptr;
    }

    if (m_secure)
    {
        m_secure->Remove();
        m_secure->DownRef();
        m_secure = nullptr;
    }

    if (m_serverSettings)
    {
        m_serverSettings->Remove();
        m_serverSettings->DownRef();
        m_serverSettings = nullptr;
    }

    TorcBonjour::TearDown();
}

TorcWebSocketThread* TorcHTTPServer::TakeSocket(TorcWebSocketThread *Socket)
{
    QMutexLocker locker(&gWebServerLock);
    if (gWebServer)
        return gWebServer->TakeSocketPriv(Socket);
    return nullptr;
}

/*! \brief Ensures remote user is authorised to access this request.
 *
 * The 'Authorization' header is used for standard HTTP requests.
 *
 * Authentication credentials supplied via the url are used for WebSocket upgrade requests - and it is assumed the if the
 * initial request is authenticated, the entire socket is then authenticated for that user.
*/
void TorcHTTPServer::Authorise(const QString &Host, TorcHTTPRequest &Request, bool ForceCheck)
{
    // N.B. the order of the following checks is critical. Always check the Authorization header
    // first and accesstokens before PreAuthorisations as PreAuthorisations are not checked again.

    // explicit authorization header
    // N.B. This will also authorise websocket clients who send authorisation headers with
    // the upgrade request i.e. there is no need to use the accesstoken route IF the correct headers
    // are sent. Use GetWebSocketToken, which requires authorisation, to force
    // clients to authenticate (i.e. browsers).
    TorcHTTPServer::AuthenticateUser(Request);
    if (Request.IsAuthorised() == HTTPAuthorised)
        return;

    if (Request.IsAuthorised() == HTTPAuthorisedStale)
    {
        AddAuthenticationHeader(Request);
        return;
    }

    // authentication token supplied in the url (WebSocket)
    // do this before 'standard' authentication to ensure token is checked and expired
    // NB ensure method is empty to stop attempts to access other resources - although
    // upgrade requests are handled before anything else, so the method should be ignored
    if (Request.Queries().contains("accesstoken") && Request.GetMethod().isEmpty())
    {
        QString token = Request.Queries().value("accesstoken");
        if (token == TorcWebSocketToken::GetWebSocketToken(Host, token))
        {
            Request.Authorise(HTTPAuthorised);
            return;
        }
    }

    // We allow unauthenticated access to anything that doesn't alter state unless auth is specifically
    // requested in the service (see TorcHTTPServices::GetWebSocketToken).
    // N.B. If a rogue peer posts to a 'setter' type function using a GET type request, the request
    // will fail during processing - so don't validate the method here as it may lead to false positives.
    // ForceCheck is used to process any authentication headers for WebSocket upgrade requests.
    HTTPRequestType type = Request.GetHTTPRequestType();
    bool authenticate = type == HTTPPost || type == HTTPPut || type == HTTPDelete || type == HTTPUnknownType;

    if (!authenticate && !ForceCheck)
    {
        Request.Authorise(HTTPPreAuthorised);
        return;
    }

    AddAuthenticationHeader(Request);
}

void TorcHTTPServer::AddAuthenticationHeader(TorcHTTPRequest &Request)
{
    Request.SetResponseType(HTTPResponseNone);
    Request.SetStatus(HTTP_Unauthorized);

    // HTTP 1.1 should support Digest
    if (true /*Request.GetHTTPProtocol() > HTTPOneDotZero*/)
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
 * Override CORS behaviour using TorcHTTPRequest::SetAllowCors (e.g. MPEG-DASH players)
*/
void TorcHTTPServer::ValidateOrigin(TorcHTTPRequest &Request)
{
    gOriginWhitelistLock.lockForRead();
    bool origin = gOriginWhitelist.contains(Request.Headers().value("Origin"), Qt::CaseInsensitive);
    gOriginWhitelistLock.unlock();

    if (Request.Headers().contains("Origin") && (origin || Request.GetAllowCORS()))
    {
        Request.SetResponseHeader("Access-Control-Allow-Origin", Request.Headers().value("Origin"));
        if (Request.Headers().contains("Access-Control-Allow-Credentials"))
            Request.SetResponseHeader("Access-Control-Allow-Credentials", "true");
        if (Request.Headers().contains("Access-Control-Request-Headers"))
            Request.SetResponseHeader("Access-Control-Request-Headers", "Origin, X-Requested-With, Content-Type, Accept, Range");
        if (Request.Headers().contains("Access-Control-Request-Method"))
            Request.SetResponseHeader("Access-Control-Request-Method", TorcHTTPRequest::AllowedToString(HTTPGet | HTTPOptions | HTTPHead));
        Request.SetResponseHeader("Access-Control-Max-Age", "86400");
    }
}

void TorcHTTPServer::AuthenticateUser(TorcHTTPRequest &Request)
{
    if (!Request.Headers().contains("Authorization"))
        return;

    QString header = Request.Headers().value("Authorization");

    // most clients will support Digest Access Authentication, so try that first
    if (header.startsWith("Digest", Qt::CaseInsensitive))
    {
        TorcHTTPServerNonce::ProcessDigestAuth(Request, true);
    }
    /* Don't use Basic. It is utterly insecure, everyone supports Digest and we store the username/password as a Digest compatible hash.
    else if (header.startsWith("Basic", Qt::CaseInsensitive))
    {
        // only accept Basic authentication if using < HTTP 1.1
        if (Request.GetHTTPProtocol() > HTTPOneDotZero)
        {
            LOG(VB_GENERAL, LOG_WARNING, "Disallowing basic authentication for HTTP 1.1 client");
        }
        else
        {
            // remove leading 'Basic' and split off token
            QString authentication = header.mid(5).trimmed();
            QStringList userinfo   = QString(QByteArray::fromBase64(authentication.toUtf8())).split(':');
            if ((userinfo.size() == 2) && (userinfo[0] == username) && (userinfo[1] == password))
                Request.Authorise(HTTPAuthorised);
        }
    }
    */
}

/*! \brief Create the 'Origin' whitelist for cross domain requests
 *
 * \note This assumes the server is listening on all interfaces (currently true).
*/
void TorcHTTPServer::UpdateOriginWhitelist(TorcHTTPServer::Status Status)
{
    QWriteLocker locker(&gOriginWhitelistLock);

    QString protocol = Status.secure ? "https://" : "http://";

    // localhost first
    gOriginWhitelist = protocol + "localhost:" + QString::number(Status.port) + " ";

    // all known raw IP addresses
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (int i = 0; i < addresses.size(); ++i)
        gOriginWhitelist += QString("%1%2 ").arg(protocol, TorcNetwork::IPAddressToLiteral(addresses[i], Status.port, false));

    // and any known host names
    QStringList hosts = TorcNetwork::GetHostNames();
    foreach (const QString &host, hosts)
    {
        QString newhost = QString("%1%2:%3 ").arg(protocol, host).arg(Status.port);
        if (!host.isEmpty() && !gOriginWhitelist.contains(newhost))
            gOriginWhitelist += newhost;
    }

    LOG(VB_NETWORK, LOG_INFO, "Origin whitelist: " + gOriginWhitelist);
}

void TorcHTTPServer::StartBonjour(void)
{
    if (!m_bonjour->GetValue().toBool())
        return;

    if (!m_ipv6->GetValue().toBool())
    {
        LOG(VB_GENERAL, LOG_INFO, "Not using Bonjour while IPv6 is disabled");
        StopBonjour();
        return;
    }

    if (!m_bonjourBrowserReference && m_bonjourSearch->GetValue().toBool())
        m_bonjourBrowserReference = TorcBonjour::Instance()->Browse("_torc._tcp.");

    if ((!m_httpBonjourReference || !m_torcBonjourReference) && m_bonjourAdvert->GetValue().toBool())
    {
        int port = m_port->GetValue().toInt();
        QMap<QByteArray,QByteArray> map;
        map.insert("uuid", gLocalContext->GetUuid().toLatin1());
        map.insert("apiversion", TorcHTTPServices::GetVersion().toLocal8Bit().constData());
        map.insert("priority",   QByteArray::number(gLocalContext->GetPriority()));
        map.insert("starttime",  QByteArray::number(gLocalContext->GetStartTime()));
        if (m_secure->GetValue().toBool())
            map.insert("secure", "yes");

        QString name = ServerDescription();

        if (!m_httpBonjourReference)
            m_httpBonjourReference = TorcBonjour::Instance()->Register(port, m_secure->GetValue().toBool() ? "_https._tcp" : "_http._tcp", name.toLocal8Bit().constData(), map);

        if (!m_torcBonjourReference)
            m_torcBonjourReference = TorcBonjour::Instance()->Register(port, "_torc._tcp", name.toLocal8Bit().constData(), map);
    }
}

void TorcHTTPServer::StopBonjour(void)
{
    StopBonjourAdvert();
    StopBonjourBrowse();
}

void TorcHTTPServer::StopBonjourBrowse(void)
{
    if (m_bonjourBrowserReference)
    {
        TorcBonjour::Instance()->Deregister(m_bonjourBrowserReference);
        m_bonjourBrowserReference = 0;
    }
}

void TorcHTTPServer::StopBonjourAdvert(void)
{
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

void TorcHTTPServer::BonjourChanged(bool Bonjour)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Bonjour %1abled").arg(Bonjour ? "en" : "dis"));

    if (Bonjour)
        StartBonjour();
    else
        StopBonjour();
}

void TorcHTTPServer::BonjourAdvertChanged(bool Advert)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Bonjour advertisement %1abled").arg(Advert ? "en" : "dis"));
    if (Advert)
        StartBonjour();
    else
        StopBonjourAdvert();
}

void TorcHTTPServer::BonjourSearchChanged(bool Search)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Bonjour search %1abled").arg(Search ? "en" : "dis"));
    if (Search)
        StartBonjour();
    else
        StopBonjourBrowse();
}

void TorcHTTPServer::IPv6Changed(bool IPv6)
{
    {
        QMutexLocker lock(&gWebServerLock);
        gWebServerStatus.ipv6 = IPv6;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("IPv6 %1abled - restarting").arg(IPv6 ? "en" : "dis"));
    QTimer::singleShot(10, this, &TorcHTTPServer::Restart);
}

bool TorcHTTPServer::Open(void)
{
    if (m_listener)
    {
        LOG(VB_GENERAL, LOG_ERR, "HTTP server alreay listening - closing");
        Close();
    }

    LOG(VB_GENERAL, LOG_INFO, QString("SSL is %1abled").arg(m_secure->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("IPv6 is %1abled").arg(m_ipv6->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("Bonjour is %1abled").arg(m_bonjour->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("Bonjour search is %1abled").arg(m_bonjourSearch->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("Bonjour advertisement is %1abled").arg(m_bonjourAdvert->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("SSDP is %1abled").arg(m_upnp->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("SSDP search is %1abled").arg(m_upnpSearch->GetValue().toBool() ? "en" : "dis"));
    LOG(VB_GENERAL, LOG_INFO, QString("SSDP advertisement is %1abled").arg(m_upnpAdvertise->GetValue().toBool() ? "en" : "dis"));
    int port = m_port->GetValue().toInt();
    LOG(VB_GENERAL, LOG_INFO, QString("Attempting to listen on port %1").arg(port));

    m_listener = new TorcHTTPServerListener(this, m_ipv6->GetValue().toBool() ? QHostAddress::Any : QHostAddress::AnyIPv4, port);
    if (!m_listener->isListening() && !m_listener->Listen(m_ipv6->GetValue().toBool() ? QHostAddress::Any : QHostAddress::AnyIPv4))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open web server port");
        Close();
        return false;
    }

    // try to use the same port
    if (port != m_listener->serverPort())
    {
        QMutexLocker locker(&gWebServerLock);
        port = m_listener->serverPort();
        m_port->SetValue(QVariant((int)port));
        gWebServerStatus.port = port;
    }

    // advertise and search
    StartBonjour();
    StartUPnP();

    LOG(VB_GENERAL, LOG_INFO, QString("Web server listening for %1secure connections on port %2").arg(m_secure->GetValue().toBool() ? "" : "in").arg(port));
    UpdateOriginWhitelist(TorcHTTPServer::GetStatus());
    return true;
}

QString TorcHTTPServer::ServerDescription(void)
{
    QString host = QHostInfo::localHostName();
    if (host.isEmpty())
        host = tr("Unknown");
    return QString("%1@%2").arg(QCoreApplication::applicationName(), host);
}

void TorcHTTPServer::Close(void)
{
    // stop advertising and searching
    StopBonjour();
    StopUPnP();

    // close connections
    m_webSocketPool.CloseSockets();

    // actually close
    if (m_listener)
        delete m_listener;
    m_listener = nullptr;

    LOG(VB_GENERAL, LOG_INFO, "Webserver closed");
}

void TorcHTTPServer::PortChanged(int Port)
{
    {
        QMutexLocker lock(&gWebServerLock);
        gWebServerStatus.port = Port;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Port changed to %1 - restarting").arg(Port));
    QTimer::singleShot(10, this, &TorcHTTPServer::Restart);
}

void TorcHTTPServer::SecureChanged(bool Secure)
{
    {
        QMutexLocker lock(&gWebServerLock);
        gWebServerStatus.secure = m_secure->GetValue().toBool();
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Secure %1abled - restarting").arg(Secure ? "en" : "dis"));
    QTimer::singleShot(10, this, &TorcHTTPServer::Restart);
}

void TorcHTTPServer::StartUPnP(void)
{
    if (m_upnp->GetValue().toBool())
    {
        bool search = m_upnpSearch->GetValue().toBool();
        bool advert = m_upnpAdvertise->GetValue().toBool();

        if (!m_ssdpThread && (search || advert))
        {
            m_ssdpThread = new TorcSSDPThread();
            m_ssdpThread->start();
        }

        if (search)
            TorcSSDP::Search(TorcHTTPServer::GetStatus());
        if (advert)
            TorcSSDP::Announce(TorcHTTPServer::GetStatus());
    }
}

void TorcHTTPServer::StopUPnP(void)
{
    TorcSSDP::CancelAnnounce();
    TorcSSDP::CancelSearch();

    if (m_ssdpThread)
    {
        m_ssdpThread->quit();
        m_ssdpThread->wait();
        delete m_ssdpThread;
        m_ssdpThread = nullptr;
    }
}

void TorcHTTPServer::UPnPChanged(bool UPnP)
{
    LOG(VB_GENERAL, LOG_INFO, QString("SSDP %1abled").arg(UPnP ? "en" : "dis"));

    if (UPnP)
        StartUPnP();
    else
        StopUPnP();
}

void TorcHTTPServer::UPnPAdvertChanged(bool Advert)
{
    LOG(VB_GENERAL, LOG_INFO, QString("SSDP advertisement %1abled").arg(Advert ? "en" : "dis"));
    if (Advert)
    {
        StartUPnP();
    }
    else
    {
        if (m_upnpSearch->GetValue().toBool())
            TorcSSDP::CancelAnnounce();
        else
            StopUPnP();
    }
}

void TorcHTTPServer::UPnPSearchChanged(bool Search)
{
    LOG(VB_GENERAL, LOG_INFO, QString("SSDP search %1abled").arg(Search ? "en" : "dis"));
    if (Search)
    {
        StartUPnP();
    }
    else
    {
        if (m_upnpAdvertise->GetValue().toBool())
            TorcSSDP::CancelSearch();
        else
            StopUPnP();
    }
}

void TorcHTTPServer::Restart(void)
{
    Close();
    Open();
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
                    UpdateOriginWhitelist(TorcHTTPServer::GetStatus());
                    return true;
                    break;
                case Torc::UserChanged:
                    LOG(VB_GENERAL, LOG_INFO, "User name/credentials changed - restarting webserver");
                    Restart();
                    return true;
                    break;
                default:
                    break;
            }
        }
    }

    return QObject::event(Event);
}

TorcWebSocketThread* TorcHTTPServer::TakeSocketPriv(TorcWebSocketThread *Socket)
{
    return m_webSocketPool.TakeSocket(Socket);
}

void TorcHTTPServer::NewConnection(qintptr SocketDescriptor)
{
    m_webSocketPool.IncomingConnection(SocketDescriptor, m_secure->GetValue().toBool());
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
        Strings.insert("ConnectedSecureTo",       QCoreApplication::translate("TorcHTTPServer", "Connected securely to"));
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

        TorcHTTPServer::gWebServer = nullptr;
    }
} TorcHTTPServerObject;


