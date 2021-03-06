/* Class TorcNetwork
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

// Torc
#include "torclocalcontext.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "http/torchttprequest.h"
#include "torcnetwork.h"
#include "torccoreutils.h"

TorcNetwork*    gNetwork = nullptr;
QMutex*         gNetworkLock = new QMutex(QMutex::Recursive);
QStringList     gNetworkHostNames;
QReadWriteLock* gNetworkHostNamesLock = new QReadWriteLock();

QPair<QHostAddress,int> gIPv4LinkLocal   = QHostAddress::parseSubnet(QStringLiteral("169.254.0.0/16"));
QPair<QHostAddress,int> gIPv4PrivateA    = QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8"));
QPair<QHostAddress,int> gIPv4PrivateB    = QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12"));
QPair<QHostAddress,int> gIPv4PrivateC    = QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16"));

QPair<QHostAddress,int> gIPv6LinkLocal   = QHostAddress::parseSubnet(QStringLiteral("fe80::/10"));
QPair<QHostAddress,int> gIPv6SiteLocal   = QHostAddress::parseSubnet(QStringLiteral("fec0::/10")); // deprecated
QPair<QHostAddress,int> gIPv6UniqueLocal = QHostAddress::parseSubnet(QStringLiteral("fc00::/7"));
QPair<QHostAddress,int> gIPv6Global      = QHostAddress::parseSubnet(QStringLiteral("2000::/3"));

bool TorcNetwork::IsAvailable(void)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->IsOnline() : false;
}

bool TorcNetwork::IsOwnAddress(const QHostAddress &Address)
{
    QMutexLocker locker(gNetworkLock);

    return gNetwork ? gNetwork->IsOwnAddressPriv(Address) : false;
}

bool TorcNetwork::Get(TorcNetworkRequest* Request)
{
    QMutexLocker locker(gNetworkLock);

    if (gNetwork->IsOnline())
    {
        emit gNetwork->NewRequest(Request);
        return true;
    }

    return false;
}

void TorcNetwork::Cancel(TorcNetworkRequest *Request)
{
    QMutexLocker locker(gNetworkLock);
    if (gNetwork)
        emit gNetwork->CancelRequest(Request);
}

void TorcNetwork::Poke(TorcNetworkRequest *Request)
{
    QMutexLocker locker(gNetworkLock);

    if (gNetwork)
        emit gNetwork->PokeRequest(Request);
}

/*! \brief Queue an asynchronous HTTP request.
 *
 * This is a method for Parent objects that are QObject subclasses residing in a full QThread.
 * The parent must implement a RequestReady(TorcNetworkRequest*) public slot, which will be invoked
 * once the download is complete. There is no need to cancel the request unless it is no longer required.
*/
bool TorcNetwork::GetAsynchronous(TorcNetworkRequest *Request, QObject *Parent)
{
    if (!Request || !Parent)
        return false;

    if (Request->m_bufferSize)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot queue asynchronous request for streamed buffer"));
        return false;
    }

    if (Parent->metaObject()->indexOfMethod(QMetaObject::normalizedSignature("RequestReady(TorcNetworkRequest*)")) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Request's parent does not have RequestReady method"));
        return false;
    }

    QMutexLocker locker(gNetworkLock);

    if (gNetwork->IsOnline())
    {
        emit gNetwork->NewAsyncRequest(Request, Parent);
        return true;
    }

    return false;
}

/*! \brief Register a known host name for this application.
 *
 * Host names are currently used to validate cross domain HTTP requests. New host names are added via reverse DNS
 * lookup when an external network connection becomes available and via successful Bonjour services registration
 * events (Bonjour typically uses a different host name).
 *
 * \sa RemoveHostName
 * \sa GetHostNames
*/
void TorcNetwork::AddHostName(const QString &Host)
{
    bool changed = false;

    {
        QWriteLocker locker(gNetworkHostNamesLock);

        if (gNetworkHostNames.size() > 10)
        {
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Number of host names > 10 - ignoring new name '%1' (%2)").arg(Host, gNetworkHostNames.join(',')));
        }
        else if (!gNetworkHostNames.contains(Host))
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("New host name '%1'").arg(Host));
            gNetworkHostNames.append(Host);
            changed = true;
        }
    }

    if (changed && gLocalContext)
        TorcLocalContext::NotifyEvent(Torc::NetworkHostNamesChanged);
}

/*! \brief Remove a host name from the known list of host names.
 *
 * \sa AddHostName
 * \sa GetHostNames
*/
void TorcNetwork::RemoveHostName(const QString &Host)
{
    bool changed = false;

    {
        QWriteLocker locker(gNetworkHostNamesLock);

        if (gNetworkHostNames.contains(Host))
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Removed host name '%1'").arg(Host));
            gNetworkHostNames.removeAll(Host);
            changed = true;
        }
    }

    if (changed && gLocalContext)
        TorcLocalContext::NotifyEvent(Torc::NetworkHostNamesChanged);
}

/*! \brief Retrieve the list of currently identified host names.
 *
 * \sa AddHostName
 * \sa RemoveHostName
 */
QStringList TorcNetwork::GetHostNames(void)
{
    QReadLocker locker(gNetworkHostNamesLock);
    QStringList result = gNetworkHostNames;
    result.removeDuplicates();
    return result;
}

/*! \brief Convert an IP address to a string literal.
 *
 * For an IPv4 address, this is a no-op. For IPv6 addresses, we need to remove the scope Id if present
 * and wrap the remainder in braces.
*/
QString TorcNetwork::IPAddressToLiteral(const QHostAddress &Address, int Port, bool UseLocalhost /* = true*/)
{
    QString result;

    if (UseLocalhost && Address.isLoopback())
    {
        result = QStringLiteral("localhost");
    }
    else if (Address.protocol() == QAbstractSocket::IPv4Protocol)
    {
        result = Address.toString();
    }
    else
    {
        QHostAddress address(Address);
        address.setScopeId(QStringLiteral(""));
        result = "[" + address.toString() +"]";
    }

    if (Port)
        result += ":" + QString::number(Port);

    return result;
}

///\brief Returns true if the address is accessible from other devices.
bool TorcNetwork::IsExternal(const QHostAddress &Address)
{
    if (Address.isNull() || Address.isLoopback())
        return false;
    return true;
}

bool TorcNetwork::IsLinkLocal(const QHostAddress &Address)
{
    if (Address.isNull())
        return false;
    if (Address.isInSubnet(Address.protocol() == QAbstractSocket::IPv4Protocol ? gIPv4LinkLocal : gIPv6LinkLocal))
        return true;
    return false;
}

bool TorcNetwork::IsLocal(const QHostAddress &Address)
{
    if (Address.isNull())
        return false;

    if (Address.protocol() == QAbstractSocket::IPv4Protocol)
        if (Address.isInSubnet(gIPv4PrivateA) || Address.isInSubnet(gIPv4PrivateB) || Address.isInSubnet(gIPv4PrivateC))
            return true;

    // IPv6 - work in progress!
    // use a whitelist approach
    if (Address.isInSubnet(gIPv6UniqueLocal) || Address.isInSubnet(gIPv6SiteLocal))
        return true;

    return false;
}

/* \brief Returns true if the address is globally accessible (i.e. exposed to the real world!)
 * \TODO make this ipv6 aware
*/
bool TorcNetwork::IsGlobal(const QHostAddress &Address)
{
    // internal/loopback and link local
    if (!IsExternal(Address))
        return false;

    // private
    if (IsLocal(Address))
        return false;

    return true;
}

void TorcNetwork::Setup(bool Create)
{
    QMutexLocker locker(gNetworkLock);

    if (gNetwork)
    {
        if (Create)
            return;

        delete gNetwork;
        gNetwork = nullptr;
        return;
    }

    if (Create)
    {
        gNetwork = new TorcNetwork();
        return;
    }

    delete gNetwork;
    gNetwork = nullptr;
}

QString ConfigurationTypeToString(QNetworkConfiguration::Type Type)
{
    switch (Type)
    {
        case QNetworkConfiguration::InternetAccessPoint: return QStringLiteral("InternetAccessPoint");
        case QNetworkConfiguration::ServiceNetwork:      return QStringLiteral("ServiceNetwork");
        case QNetworkConfiguration::UserChoice:          return QStringLiteral("UserDefined");
        case QNetworkConfiguration::Invalid:             return QStringLiteral("Invalid");
    }

    return QStringLiteral();
}

/*! \class TorcNetwork
 *  \brief Subclass of QNetworkAccessManager for sending network requests and monitoring the network state.
 *
 * \todo Check whether authenticationRequired signal is being emitted.
*/
TorcNetwork::TorcNetwork()
  : QNetworkAccessManager(),
    m_online(false),
    m_manager(this),
    m_configuration(),
    m_hostNames(),
    m_requests(),
    m_reverseRequests(),
    m_asynchronousRequests()
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Opening network access manager"));
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSL support is %1available").arg(QSslSocket::supportsSsl() ? QStringLiteral("") : QStringLiteral("not ")));

    connect(&m_manager, &QNetworkConfigurationManager::configurationAdded,   this, &TorcNetwork::ConfigurationAdded);
    connect(&m_manager, &QNetworkConfigurationManager::configurationChanged, this, &TorcNetwork::ConfigurationChanged);
    connect(&m_manager, &QNetworkConfigurationManager::configurationRemoved, this, &TorcNetwork::ConfigurationRemoved);
    connect(&m_manager, &QNetworkConfigurationManager::onlineStateChanged,   this, &TorcNetwork::OnlineStateChanged);
    connect(&m_manager, &QNetworkConfigurationManager::updateCompleted,      this, &TorcNetwork::UpdateCompleted);

    connect(this, &TorcNetwork::NewRequest,    this, &TorcNetwork::GetSafe);
    connect(this, &TorcNetwork::CancelRequest, this, &TorcNetwork::CancelSafe);
    connect(this, &TorcNetwork::PokeRequest,   this, &TorcNetwork::PokeSafe);
    connect(this, &TorcNetwork::NewAsyncRequest, this, &TorcNetwork::GetAsynchronousSafe);

    // direct connection for authentication requests
    connect(this, &TorcNetwork::authenticationRequired, this, &TorcNetwork::Authenticate, Qt::DirectConnection);

    // set initial state
    UpdateConfiguration(true);
}

TorcNetwork::~TorcNetwork()
{
    // release any outstanding requests
    CloseConnections();

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Closing network access manager"));
}

bool TorcNetwork::IsOnline(void)
{
    return m_online;
}

bool TorcNetwork::IsOwnAddressPriv(const QHostAddress &Address)
{
    return QNetworkInterface::allAddresses().contains(Address);
}

void TorcNetwork::GetSafe(TorcNetworkRequest* Request)
{
    if (Request && m_online)
    {
        Request->UpRef();

        // some servers require a recognised user agent...
        Request->m_request.setRawHeader("User-Agent", DEFAULT_USER_AGENT);

        QNetworkReply* reply = nullptr;

        if (Request->m_type == QNetworkAccessManager::GetOperation)
            reply = get(Request->m_request);
        else if (Request->m_type == QNetworkAccessManager::HeadOperation)
            reply = head(Request->m_request);
        else if (Request->m_type == QNetworkAccessManager::PostOperation)
            reply = post(Request->m_request, Request->m_postData);

        if (!reply)
        {
            Request->DownRef();
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unknown request type"));
            return;
        }

        // join the dots
        connect(reply, &QNetworkReply::readyRead,        this, &TorcNetwork::ReadyRead);
        connect(reply, &QNetworkReply::finished,         this, &TorcNetwork::Finished);
        connect(reply, &QNetworkReply::downloadProgress, this, &TorcNetwork::DownloadProgress);
        connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), this, &TorcNetwork::Error);
        connect(reply, &QNetworkReply::sslErrors,        this, &TorcNetwork::SSLErrors);

        m_requests.insert(reply, Request);
        m_reverseRequests.insert(Request, reply);
    }
}

void TorcNetwork::CancelSafe(TorcNetworkRequest *Request)
{
    if (m_reverseRequests.contains(Request))
    {
        QNetworkReply* reply = m_reverseRequests.take(Request);
        Request->m_rawHeaders = reply->rawHeaderPairs();
        m_requests.remove(reply);
        if (reply->isFinished())
            LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Deleting finished request '%1'").arg(reply->request().url().toString()));
        else
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Cancelling '%1'").arg(reply->request().url().toString()));
        reply->abort();
        reply->deleteLater();
        Request->DownRef();

        if (m_asynchronousRequests.contains(Request))
        {
            QObject *parent = m_asynchronousRequests.take(Request);
            if (!QMetaObject::invokeMethod(parent, "RequestReady", Qt::AutoConnection, Q_ARG(TorcNetworkRequest*, Request)))
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending RequestReady"));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Trying to cancel unknown network request"));
    }
}

void TorcNetwork::PokeSafe(TorcNetworkRequest *Request)
{
    if (m_reverseRequests.contains(Request))
        Request->Write(m_reverseRequests.value(Request));
}

void TorcNetwork::GetAsynchronousSafe(TorcNetworkRequest *Request, QObject *Parent)
{
    if (!Request || !Parent)
        return;

    if (m_asynchronousRequests.contains(Request))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Asynchronous request is already queued - ignoring"));
        return;
    }

    if (m_online)
    {
        m_asynchronousRequests.insert(Request, Parent);
        GetSafe(Request);
    }
}

void TorcNetwork::ConfigurationAdded(const QNetworkConfiguration &Config)
{
    (void)Config;
    UpdateConfiguration();
}

void TorcNetwork::ConfigurationChanged(const QNetworkConfiguration &Config)
{
    (void)Config;
    UpdateConfiguration();
}

void TorcNetwork::ConfigurationRemoved(const QNetworkConfiguration &Config)
{
    (void)Config;
    UpdateConfiguration();
}

void TorcNetwork::OnlineStateChanged(bool Online)
{
    (void)Online;
    UpdateConfiguration();
}

void TorcNetwork::UpdateCompleted(void)
{
    UpdateConfiguration();
}

bool TorcNetwork::CheckHeaders(TorcNetworkRequest *Request, QNetworkReply *Reply)
{
    if (!Request || !Reply)
        return false;

    QVariant status = Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!status.isValid())
        return true;

    int httpstatus    = status.toInt();
    int contentlength = 0;

    // content length
    QVariant length = Reply->header(QNetworkRequest::ContentLengthHeader);
    if (length.isValid())
    {
        int size = length.toInt();
        if (size > 0)
            contentlength = size;
    }

    // content type
    QVariant contenttype = Reply->header(QNetworkRequest::ContentTypeHeader);

    if (Request->m_type == QNetworkAccessManager::HeadOperation)
    {
        // NB the following assumes the head request is checking for byte serving support

        // some servers (yes - I'm talking to you Dropbox) don't allow HEAD requests for
        // some files (secure redirections?). Furthermore they don't return a valid Allow list
        // in the response, or in response to an OPTIONS request. We could go around in circles
        // trying to check support in a number of ways, but just cut to the chase and issue
        // a test range request (as they do actually support range requests)
        if (httpstatus == HTTP_MethodNotAllowed)
        {
            // delete the reply and try again as a GET request with range
            m_reverseRequests.remove(Request);
            m_requests.remove(Reply);
            Request->m_request.setUrl(Reply->request().url());
            Request->m_type = QNetworkAccessManager::GetOperation;
            Request->SetRange(0, 10);
            Reply->abort();
            Reply->deleteLater();
            GetSafe(Request);
            Request->DownRef();
            return false;
        }
    }

    Request->m_httpStatus = httpstatus;
    Request->m_contentLength = contentlength;
    Request->m_contentType = contenttype.isValid() ? contenttype.toString().toLower() : QStringLiteral();
    Request->m_byteServingAvailable = Reply->rawHeader("Accept-Ranges").toLower().contains("bytes") && contentlength > 0;
    return true;
}

bool TorcNetwork::Redirected(TorcNetworkRequest *Request, QNetworkReply *Reply)
{
    if (!Request || !Reply)
        return false;

    QUrl newurl = Reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    QUrl oldurl = Request->m_request.url();

    if (!newurl.isEmpty() && newurl != oldurl)
    {
        // redirected
        if (newurl.isRelative())
            newurl = oldurl.resolved(newurl);

        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Redirected from '%1' to '%2'").arg(oldurl.toString(), newurl.toString()));
        if (Request->m_redirectionCount++ < DEFAULT_MAX_REDIRECTIONS)
        {
            // delete the reply and create a new one
            m_reverseRequests.remove(Request);
            m_requests.remove(Reply);
            Reply->abort();
            Reply->deleteLater();
            Request->m_request.setUrl(newurl);
            GetSafe(Request);
            Request->DownRef();
            return true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Max redirections exceeded"));
        }
    }

    return false;
}

void TorcNetwork::ReadyRead(void)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    TorcNetworkRequest* request = nullptr;
    if (reply && m_requests.contains(reply) && (request = m_requests.value(reply)))
    {
        if (!request->m_started)
        {
            // check for redirection
            if (Redirected(request, reply))
                return;

            // no need to check return value for GET requests
            (void)CheckHeaders(request, reply);

            // we need to set the buffer size after the download has started as Qt will ignore
            // the set value if it doesn't yet know the expected size. Not ideal...
            if (request->m_bufferSize)
                reply->setReadBufferSize(request->m_bufferSize);

            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Download started"));
            request->m_started = true;
        }

        request->Write(reply);
    }
}

void TorcNetwork::Finished(void)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    TorcNetworkRequest *request = nullptr;
    if (reply && m_requests.contains(reply) && (request = m_requests.value(reply)))
    {
        if (request->m_type == QNetworkAccessManager::HeadOperation)
        {
            // head requests never trigger a read request (no content), so check redirection on completion
            if (Redirected(request, reply))
                return;

            // a false return value indicates the request has been resent (perhaps in another form)
            if (!CheckHeaders(request, reply))
                return;
        }
        else if (request->m_type == QNetworkAccessManager::PostOperation)
        {
            // as for head, Post ops responses may have no content and hence ReadyRead is never triggered
            // so check headers now (to set status etc)
            (void)CheckHeaders(request, reply);
        }

        request->m_replyFinished = true;

        // we need to manage async requests
        if (m_asynchronousRequests.contains(request))
            CancelSafe(request);
    }
}

void TorcNetwork::Error(QNetworkReply::NetworkError Code)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    TorcNetworkRequest *request = nullptr;
    if (reply && m_requests.contains(reply) && (request = m_requests.value(reply)) && (Code != QNetworkReply::OperationCanceledError))
    {
        request->SetReplyError(Code);
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Network error '%1'").arg(reply->errorString()));
    }
}

QList<QSslError> TorcNetwork::AllowableSslErrors(const QList<QSslError> &Errors)
{
    bool selfsigned = false;
    bool mismatched = false;
    QSslError mismatch;

    QList<QSslError> allowed;
    foreach (const QSslError &error, Errors)
    {
        if (error.error() == QSslError::SelfSignedCertificate)
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Allowing self signed certificate"));
            allowed << error;
            selfsigned = true;
        }
        else if (error.error() == QSslError::HostNameMismatch)
        {
            mismatched = true;
            mismatch   = error;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Ssl Error: %1").arg(error.errorString()));
        }
    }

    if (mismatched)
    {
        if (selfsigned)
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Allowing host name mismatch for self signed certfificate"));
            allowed << mismatch;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Ssl Error: %1").arg(mismatch.errorString()));
        }
    }

    return allowed;
}

void TorcNetwork::SSLErrors(const QList<QSslError> &Errors)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());
    if (reply)
    {
        QList<QSslError> allowed = TorcNetwork::AllowableSslErrors(Errors);
        if (!allowed.isEmpty())
            reply->ignoreSslErrors(TorcNetwork::AllowableSslErrors(Errors));
    }
}

void TorcNetwork::DownloadProgress(qint64 Received, qint64 Total)
{
    QNetworkReply *reply = dynamic_cast<QNetworkReply*>(sender());

    TorcNetworkRequest *request = nullptr;
    if (reply && m_requests.contains(reply) && (request = m_requests.value(reply)))
        request->DownloadProgress(Received, Total);
}

void TorcNetwork::Authenticate(QNetworkReply *Reply, QAuthenticator *Authenticator)
{
    (void)Reply;
    (void)Authenticator;
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Authentication required"));
}

///\brief Receives host name updates from QHostInfo
void TorcNetwork::NewHostName(const QHostInfo &Info)
{
    if (!Info.hostName().isEmpty())
    {
        m_hostNames.append(Info.hostName());
        AddHostName(Info.hostName());
    }
}

/*! \brief Cancel all current network requests.
 *
 * This method can be called when the TorcNetwork singleton is being destroyed, when network access has
 * been disallowed or when the network is down. It is reasonable to expect outstanding requests in the latter 2
 * cases but well behaved clients should have cancelled any requests in the first case. Hence we warn in this
 * instance.
 *
 * \note Command line applications currently do not run an admin thread and call QCoreApplication::quit to
 * close the application. In this specific case, the main event loop is no longer running when clients call
 * TorcNetwork::Cancel, the CancelRequest signal is never sent/received and requests may still be outstanding.
*/
void TorcNetwork::CloseConnections(void)
{
    if (!m_requests.isEmpty())
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("%1 outstanding network requests").arg(m_requests.size()));

    while (!m_requests.isEmpty())
        CancelSafe(*m_requests.begin());

    m_requests.clear();
    m_reverseRequests.clear();
    m_asynchronousRequests.clear();
}

void TorcNetwork::UpdateConfiguration(bool Creating)
{
    QNetworkConfiguration configuration = m_manager.defaultConfiguration();
    bool wasonline = m_online;
    bool changed = false;

    if (configuration != m_configuration || Creating)
    {
        changed = true;
        m_configuration = configuration;

        if (m_configuration.isValid())
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Network interface: %1 Bearer: %2").arg(configuration.name(), configuration.bearerTypeName()));
            m_online = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("No valid network connection"));
            m_online = false;
        }
    }

    if (m_online && !wasonline)
    {
        TorcLocalContext::NotifyEvent(Torc::NetworkAvailable);

        QStringList addresses;
        QList<QHostAddress> entries = QNetworkInterface::allAddresses();
        foreach (const QHostAddress &entry, entries)
        {
            QString address = entry.toString();
            addresses << address;
            QHostInfo::lookupHost(address, this, SLOT(NewHostName(QHostInfo)));
        }

        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Network up (%1)").arg(addresses.join(QStringLiteral(", "))));
    }
    else if (wasonline && !m_online)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Network down"));
        CloseConnections();
        TorcLocalContext::NotifyEvent(Torc::NetworkUnavailable);

        foreach (QString host, m_hostNames)
            RemoveHostName(host);
        m_hostNames = QStringList();
    }
    else if (changed)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Network configuration changed"));
        TorcLocalContext::NotifyEvent(Torc::NetworkChanged);
    }
}

/*! \class TorcNetworkObject
 *  \brief A static class used to create the TorcNetwork singleton in the admin thread.
*/
class TorcNetworkObject : public TorcAdminObject
{
  public:
    TorcNetworkObject()
      : TorcAdminObject(TORC_ADMIN_CRIT_PRIORITY)
    {
        qRegisterMetaType<TorcNetworkRequest*>();
        qRegisterMetaType<QNetworkReply*>();
    }

    void Create(void)
    {
        // always create the network object to at least monitor network state.
        // if access is disallowed, it will be made inaccessible.
        TorcNetwork::Setup(true);
    }

    void Destroy(void)
    {
        TorcNetwork::Setup(false);
    }
} TorcNetworkObject;


