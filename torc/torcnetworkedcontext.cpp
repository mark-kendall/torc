/* Class TorcNetworkedContext
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013-18
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
#include "torcadminthread.h"
#include "torclanguage.h"
#include "torcnetwork.h"
#include "torcbonjour.h"
#include "torcevent.h"
#include "upnp/torcupnp.h"
#include "torcwebsocket.h"
#include "torcrpcrequest.h"
#include "torcnetworkrequest.h"
#include "torcwebsocketthread.h"
#include "torchttpserver.h"
#include "torcnetworkedcontext.h"

TorcNetworkedContext *gNetworkedContext = nullptr;

/*! \class TorcNetworkService
 *  \brief Encapsulates information on a discovered Torc peer.
 *
 * \todo Should retries be limited? If support is added for manually specified peers (e.g. remote) and that
 *       peer is offline, need a better approach.
*/
TorcNetworkService::TorcNetworkService(const QString &Name, const QString &UUID, int Port,
                                       bool Secure, const QList<QHostAddress> &Addresses)
  : QObject(),
    name(Name),
    uuid(UUID),
    port(Port),
    host(),
    secure(Secure),
    uiAddress(QString()),
    startTime(0),
    priority(-1),
    apiVersion(QString()),
    connected(false),
    m_sources(Spontaneous),
    m_debugString(),
    m_addresses(Addresses),
    m_preferredAddressIndex(0),
    m_abort(0),
    m_getPeerDetailsRPC(nullptr),
    m_getPeerDetails(nullptr),
    m_webSocketThread(nullptr),
    m_retryScheduled(false),
    m_retryInterval(10000)
{
    for (int i = 0; i < m_addresses.size(); ++i)
    {
        if (m_addresses.at(i).protocol() == QAbstractSocket::IPv4Protocol)
            m_preferredAddressIndex = i;

        uiAddress += TorcNetwork::IPAddressToLiteral(m_addresses.at(i), port) + ((i == m_addresses.size() - 1) ? "" : ", ");
    }

    if (m_addresses.isEmpty())
        m_debugString = "No address found";
    else
        m_debugString = TorcNetwork::IPAddressToLiteral(m_addresses[m_preferredAddressIndex], port);

    connect(this, SIGNAL(TryConnect()), this, SLOT(Connect()));
}

/*! \brief Destroy this service.
 *
 * \note When cancelling m_getPeerDetailsRPC, the call to TorcWebSocket::CancelRequest is blocking (to
 *  avoid leaking the request). Hence the request should always have been cancelled and downref'd when
 *  the call is complete. If the request happens to be processed while it is being cancelled, the request
 *  will no longer be valid for cancellation in the TorcWebSocket thread and if RequestReady is triggered in the
 *  TorcNetworkedContext thread (main thread), m_getPeerDetailsRPC will already have been released.
 *  This is because TorcWebSocket and TorcNetworkedContext process all of their updates in their own threads.
 *
 * \todo Make the call to TorcNetwork::Cancel blocking, as for TorcWebSocket::CancelRequest.
*/
TorcNetworkService::~TorcNetworkService()
{
    // cancel any outstanding requests
    m_abort = 1;

    if (m_getPeerDetailsRPC && m_webSocketThread)
    {
        m_webSocketThread->CancelRequest(m_getPeerDetailsRPC);
        m_getPeerDetailsRPC->DownRef();
        m_getPeerDetailsRPC = nullptr;
    }

    if (m_getPeerDetails)
    {
        TorcNetwork::Cancel(m_getPeerDetails);
        m_getPeerDetails->DownRef();
        m_getPeerDetails = nullptr;
    }

    // delete websocket
    if (m_webSocketThread)
    {
        m_webSocketThread->quit();
        m_webSocketThread->wait();
        delete m_webSocketThread;
        m_webSocketThread = nullptr;
    }
}

/*! \brief Determine whether we (the local device) should be the server for peer to peer communications.
 *
 * The server is the instance with:
 *  - higher priority
 *  - same priority but earlier start time
 *  - same priority and start time but 'bigger' UUID
*/
bool TorcNetworkService::WeActAsServer(int Priority, qint64 StartTime, const QString &UUID)
{
    if (Priority != gLocalContext->GetPriority())
        return Priority < gLocalContext->GetPriority();
    if (StartTime != gLocalContext->GetStartTime())
        return StartTime > gLocalContext->GetStartTime();
    if (UUID != gLocalContext->GetUuid())
        return UUID < gLocalContext->GetUuid();
    return false; // at least both devices will try and connect...
}

/*! \brief Establish a WebSocket connection to the peer if necessary.
 *
 * A connection is only established if we have the necessary details (address, API version, priority,
 * start time etc) and this application should act as the client.
 *
 * A client has a lower priority or later start time in the event of matching priorities.
*/
void TorcNetworkService::Connect(void)
{
    // clear retry flag
    m_retryScheduled = false;

    // sanity check
    if (m_addresses.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "No valid peer addresses");
        return;
    }

    // NB all connection methods should now provide all necessary peer data. QueryPeerDetails should now be
    // redundant.
    if (startTime == 0 || apiVersion.isEmpty() || priority < 0)
    {
        QueryPeerDetails();
        return;
    }

    // already connected
    if (m_webSocketThread)
    {
        // notify the parent that the connection is complete
        if (gNetworkedContext)
            gNetworkedContext->Connected(this);

        connected = true;
        emit ConnectedChanged();

        return;
    }

    if (WeActAsServer(priority, startTime, uuid))
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Not connecting to %1 - we have priority").arg(m_debugString));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Trying to connect to %1").arg(m_debugString));

    m_webSocketThread = new TorcWebSocketThread(m_addresses.at(m_preferredAddressIndex), port, secure);
    connect(m_webSocketThread, SIGNAL(Finished()),           this, SLOT(Disconnected()));
    connect(m_webSocketThread, SIGNAL(ConnectionUpgraded()), this, SLOT(Connected()));

    m_webSocketThread->start();
}

QString TorcNetworkService::GetName(void)
{
    return name;
}

QString TorcNetworkService::GetUuid(void)
{
    return uuid;
}

int TorcNetworkService::GetPort(void)
{
    return port;
}

QString TorcNetworkService::GetHost(void)
{
    return host;
}

QString TorcNetworkService::GetAddress(void)
{
    return uiAddress;
}

qint64 TorcNetworkService::GetStartTime(void)
{
    return startTime;
}

int TorcNetworkService::GetPriority(void)
{
    return priority;
}

QString TorcNetworkService::GetAPIVersion(void)
{
    return apiVersion;
}

bool TorcNetworkService::GetConnected(void)
{
    return connected;
}

void TorcNetworkService::Connected(void)
{
    TorcWebSocketThread *thread = static_cast<TorcWebSocketThread*>(sender());
    if (m_webSocketThread && m_webSocketThread == thread)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Connection established with %1").arg(m_debugString));
        emit TryConnect();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown WebSocket connected...");
    }
}

void TorcNetworkService::Disconnected(void)
{
    TorcWebSocketThread *thread = static_cast<TorcWebSocketThread*>(sender());
    if (m_webSocketThread && m_webSocketThread == thread)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Connection with %1 closed").arg(m_debugString));
        m_webSocketThread->quit();
        m_webSocketThread->wait();
        delete m_webSocketThread;
        m_webSocketThread = nullptr;

        // try and reconnect. If this is a discovered service, the socket was probably closed
        // deliberately and this object is about to be deleted anyway.
        ScheduleRetry();

        // notify the parent
        if (gNetworkedContext)
        {
            gNetworkedContext->Disconnected(this);

            connected = false;
            emit ConnectedChanged();
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown WebSocket disconnected...");
    }
}

void TorcNetworkService::RequestReady(TorcNetworkRequest *Request)
{
    if (m_getPeerDetails && (Request == m_getPeerDetails))
    {
        QJsonDocument jsonresult = QJsonDocument::fromJson(Request->GetBuffer());
        m_getPeerDetails->DownRef();
        m_getPeerDetails = nullptr;

        if (!jsonresult.isNull() && jsonresult.isObject())
        {
            QJsonObject object = jsonresult.object();

            if (object.contains("details"))
            {
                QJsonObject   details   = object["details"].toObject();
                QJsonValueRef jpriority  = details["priority"];
                QJsonValueRef jstarttime = details["starttime"];
                QJsonValueRef jversion   = details["version"];

                if (!jpriority.isNull() && !jstarttime.isNull() && !jversion.isNull())
                {
                    apiVersion = jversion.toString();
                    priority   = (int)jpriority.toDouble();
                    startTime  = (qint64)jstarttime.toDouble();

                    emit ApiVersionChanged();
                    emit PriorityChanged();
                    emit StartTimeChanged();

                    emit TryConnect();
                    return;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, "Failed to retrieve peer information");
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to find 'details' in peer response");
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Error parsing API return - expecting JSON object");
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Response:\r\n%1").arg(jsonresult.toJson().data()));

        // try again...
        ScheduleRetry();
    }
}

void TorcNetworkService::RequestReady(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    if (Request == m_getPeerDetailsRPC)
    {
        bool success = false;
        QString message;
        int state = m_getPeerDetailsRPC->GetState();

        if (state & TorcRPCRequest::TimedOut)
        {
            message = "Timed out";
        }
        else if (state & TorcRPCRequest::Cancelled)
        {
            message = "Cancelled";
        }
        else if (state & TorcRPCRequest::Errored)
        {
            QVariantMap map  = m_getPeerDetailsRPC->GetReply().toMap();
            QVariant error = map["error"];
            if (error.type() == QVariant::Map)
            {
                QVariantMap errors = error.toMap();
                message = QString("'%1' (%2)").arg(errors.value("message").toString()).arg(errors.value("code").toInt());
            }
        }
        else if (state & TorcRPCRequest::ReplyReceived)
        {
            if (m_getPeerDetailsRPC->GetReply().type() == QVariant::Map)
            {
                QVariantMap map     = m_getPeerDetailsRPC->GetReply().toMap();
                QVariant vpriority  = map["priority"];
                QVariant vstarttime = map["starttime"];
                QVariant vversion   = map["version"];

                if (!vpriority.isNull() && !vstarttime.isNull() && !vversion.isNull())
                {
                    apiVersion = vversion.toString();
                    priority   = (int)vpriority.toDouble();
                    startTime  = (qint64)vstarttime.toDouble();

                    emit ApiVersionChanged();
                    emit PriorityChanged();
                    emit StartTimeChanged();

                    success = true;
                }
                else
                {
                    message = "Incomplete details";
                }
            }
            else
            {
                message = QString("Unexpected variant type %1").arg(m_getPeerDetailsRPC->GetReply().type());
            }
        }

        if (!success)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Call to '%1' failed (%2)").arg(m_getPeerDetailsRPC->GetMethod()).arg(message));
            ScheduleRetry();
        }

        m_getPeerDetailsRPC->DownRef();
        m_getPeerDetailsRPC = nullptr;

        if (success)
        {
            LOG(VB_GENERAL, LOG_INFO, "Reply received");
            emit TryConnect();
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown request ready");
    }
}

void TorcNetworkService::ScheduleRetry(void)
{
    if (!m_retryScheduled)
    {
        QTimer::singleShot(m_retryInterval, Qt::VeryCoarseTimer, this, SLOT(Connect()));
        m_retryScheduled = true;
    }
}

void TorcNetworkService::QueryPeerDetails(void)
{
    // this is a private method only called from Connect. No need to validate m_addresses or current details.

    if (!m_webSocketThread)
    {
        if (m_getPeerDetails)
        {
            LOG(VB_GENERAL, LOG_ERR, "Already running GetDetails HTTP request");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "Querying peer details over HTTP");

        QUrl url;
        url.setScheme(secure ? "https" : "http");
        url.setPort(port);
        url.setHost(m_addresses[m_preferredAddressIndex].toString());
        url.setPath("/services/GetDetails");
        QNetworkRequest networkrequest(url);
        networkrequest.setRawHeader("Accept", "application/json");

        m_getPeerDetails = new TorcNetworkRequest(networkrequest, QNetworkAccessManager::GetOperation, 0, &m_abort);
        TorcNetwork::GetAsynchronous(m_getPeerDetails, this);

        return;
    }

    // perform JSON-RPC call over socket
    if (m_getPeerDetailsRPC)
    {
        LOG(VB_GENERAL, LOG_ERR, "Already running GetDetails RPC");
        return;
    }

    m_getPeerDetailsRPC = new TorcRPCRequest("/services/GetDetails", this);
    RemoteRequest(m_getPeerDetailsRPC);
}

void TorcNetworkService::SetWebSocketThread(TorcWebSocketThread *Thread)
{
    if (!Thread)
        return;

    // guard against incorrect use
    if (m_webSocketThread)
    {
        LOG(VB_GENERAL, LOG_ERR, "Already have websocket - closing new socket");
        Thread->quit();
        return;
    }

    // create the socket
    m_webSocketThread = Thread;
    connect(m_webSocketThread, SIGNAL(Finished()), this, SLOT(Disconnected()));
}

void TorcNetworkService::RemoteRequest(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    if (m_webSocketThread)
    {
        m_webSocketThread->RemoteRequest(Request);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot fulfill remote request - not connected");
        Request->DownRef();
    }
}

void TorcNetworkService::CancelRequest(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    if (m_webSocketThread)
        m_webSocketThread->CancelRequest(Request);
    else
        LOG(VB_GENERAL, LOG_ERR, "Cannot cancel request - not connected");
}

QVariant TorcNetworkService::ToMap(void)
{
    QVariantMap result;
    result.insert("name",      name);
    result.insert("uuid",      uuid);
    result.insert("port",      port);
    result.insert("uiAddress", uiAddress);
    result.insert("address",   m_addresses.isEmpty() ? "INValid" : TorcNetwork::IPAddressToLiteral(m_addresses[m_preferredAddressIndex], 0));
    result.insert("host",      host);
    if (secure)
        result.insert("secure", "yes");
    return result;
}

QList<QHostAddress> TorcNetworkService::GetAddresses(void)
{
    return m_addresses;
}

void TorcNetworkService::SetHost(const QString &Host)
{
    host = Host;
    m_debugString = host + ":" + QString::number(port);
}

void TorcNetworkService::SetStartTime(qint64 StartTime)
{
    startTime = StartTime;
}

void TorcNetworkService::SetPriority(int Priority)
{
    priority = Priority;
}

void TorcNetworkService::SetAPIVersion(const QString &Version)
{
    apiVersion = Version;
}

void TorcNetworkService::SetSource(ServiceSource Source)
{
    m_sources |= Source;
}

TorcNetworkService::ServiceSources TorcNetworkService::GetSources(void)
{
    return m_sources;
}

void TorcNetworkService::RemoveSource(ServiceSource Source)
{
    m_sources &= !Source;
}

#define BLACKLIST QString("submit,revert")

/*! \class TorcNetworkedContext
 *  \brief A class to discover and connect to other Torc applications.
 *
 * ![](../images/peer-decisiontree.svg) "Torc peer discovery and connection"
 *
 * TorcNetworkedContext searches for other Torc applications on the local network using UPnP and Bonjour (where available).
 * When a new application is discovered, its details will be retrieved automatically and a websocket connection initiated
 * if necessary.
 *
 * The list of detected peers is available to the local UI via QAbstractListModel and hence this object must sit in the
 * main thread. The peer list is also available to remote UIs via TorcHTTPService, which will process requests from multiple
 * threads, hence limited locking is required around the peer list (the remote UI cannot write to the peer list, only read).
 *
 * \sa TorcNetworkService
 */
TorcNetworkedContext::TorcNetworkedContext()
  : QObject(),
    TorcHTTPService(this, "peers", "peers", TorcNetworkedContext::staticMetaObject, BLACKLIST),
    m_discoveredServices(),
    m_serviceList(),
    peers()
{
    // listen for events
    gLocalContext->AddObserver(this);

    // connect signals
    connect(this, SIGNAL(NewRequest(QString,TorcRPCRequest*)), this, SLOT(HandleNewRequest(QString,TorcRPCRequest*)));
    connect(this, SIGNAL(RequestCancelled(QString,TorcRPCRequest*)), this, SLOT(HandleCancelRequest(QString,TorcRPCRequest*)));
    connect(this, SIGNAL(NewPeer(TorcWebSocketThread*,QVariantMap)), this, SLOT(HandleNewPeer(TorcWebSocketThread*,QVariantMap)));

    // always create the global instance
    TorcBonjour::Instance();

    // NB all searches are triggered from TorcHTTPServer
}

TorcNetworkedContext::~TorcNetworkedContext()
{
    // stoplistening
    gLocalContext->RemoveObserver(this);

    if (!m_discoveredServices.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, QString("Removing %1 discovered peers").arg(m_discoveredServices.size()));
    while (!m_discoveredServices.isEmpty())
        delete m_discoveredServices.takeLast();
}

QString TorcNetworkedContext::GetUIName(void)
{
    return tr("Peers");
}

QVariantList TorcNetworkedContext::GetPeers(void)
{
    QVariantList result;

    QReadLocker locker(&m_httpServiceLock);
    foreach (TorcNetworkService* service, m_discoveredServices)
        result.append(service->ToMap());

    return result;
}

void TorcNetworkedContext::Connected(TorcNetworkService *Peer)
{
    if (!Peer)
        return;

    emit PeerConnected(Peer->GetName(), Peer->GetUuid());
}

void TorcNetworkedContext::Disconnected(TorcNetworkService *Peer)
{
    if (!Peer)
        return;

    emit PeerDisconnected(Peer->GetName(), Peer->GetUuid());

    // we need to delete the service if it was not discovered (i.e. a disconnection means it went away
    // and we will receive no other notifications).
    Remove(Peer->GetUuid());
}

bool TorcNetworkedContext::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent *event = static_cast<TorcEvent*>(Event);
        if (event && (event->GetEvent() == Torc::ServiceDiscovered || event->GetEvent() == Torc::ServiceWentAway))
        {
            if (event->Data().contains("txtrecords"))
            {
                // txtrecords is Bonjour specific
                QMap<QByteArray,QByteArray> records = TorcBonjour::TxtRecordToMap(event->Data().value("txtrecords").toByteArray());

                if (records.contains("uuid"))
                {
                    QByteArray uuid  = records.value("uuid");

                    if (event->GetEvent() == Torc::ServiceDiscovered)
                    {
                        if (uuid == gLocalContext->GetUuid().toLatin1())
                        {
                            // this is our own external Bonjour host name
                            TorcNetwork::AddHostName(event->Data().value("host").toString());
                        }
                        else if (m_serviceList.contains(uuid))
                        {
                            // register this as an additional source
                            QWriteLocker locker(&m_httpServiceLock);
                            for (int i = 0; i < m_discoveredServices.size(); ++i)
                                if (m_discoveredServices.at(i)->GetUuid() == uuid)
                                    m_discoveredServices.at(i)->SetSource(TorcNetworkService::Bonjour);
                        }
                        else
                        {
                            QStringList addresses = event->Data().value("addresses").toStringList();

                            QList<QHostAddress> hosts;
                            foreach (QString address, addresses)
                            {
                                // filter out link local addresses for external peers that have network problems...
                                // but allow loopback addresses for other Torc instances running on the same device...
                                QHostAddress hostaddress(address);
                                if (!TorcNetwork::IsExternal(hostaddress) && !hostaddress.isLoopback())
                                {
                                    LOG(VB_GENERAL, LOG_INFO, QString("Ignoring peer address '%1' - address is unreachable").arg(address));
                                }
                                else
                                {
                                    hosts.append(QHostAddress(address));
                                }
                            }

                            QString host = event->Data().value("host").toString();

                            if (hosts.isEmpty())
                            {
                                LOG(VB_GENERAL, LOG_WARNING, QString("Ignoring peer '%1' - no useable network addresses.").arg(host));
                            }
                            else
                            {
                                QString name = event->Data().value("name").toString();
                                QByteArray txtrecords = event->Data().value("txtrecords").toByteArray();
                                QMap<QByteArray,QByteArray> map = TorcBonjour::TxtRecordToMap(txtrecords);
                                QString version       = QString(map.value("apiversion"));
                                qint64 starttime      = map.value("starttime").toULongLong();
                                int priority          = map.value("priority").toInt();
                                bool secure           = map.contains("secure");

                                // create the new peer
                                TorcNetworkService *service = new TorcNetworkService(name, uuid, event->Data().value("port").toInt(),
                                                                                     secure, hosts);
                                service->SetAPIVersion(version);
                                service->SetPriority(priority);
                                service->SetStartTime(starttime);
                                service->SetHost(host);
                                service->SetSource(TorcNetworkService::Bonjour);

                                // and insert into the list model
                                Add(service);

                                // try and connect - the txt records should have given us everything we need to know
                                emit service->TryConnect();
                            }
                        }
                    }
                    else if (event->GetEvent() == Torc::ServiceWentAway)
                    {
                        if (uuid == gLocalContext->GetUuid().toLatin1())
                            TorcNetwork::RemoveHostName(event->Data().value("host").toString());
                        else
                            Remove(uuid, TorcNetworkService::Bonjour);
                    }
                }
            }
            else if (event->Data().contains("usn"))
            {
                // USN == Unique Service Name (UPnP)
                QString usn      = event->Data().value("usn").toString();
                QString uuid     = TorcUPNP::UUIDFromUSN(usn);

                if (event->GetEvent() == Torc::ServiceWentAway)
                {
                    Remove(uuid, TorcNetworkService::UPnP);
                }
                else if (event->GetEvent() == Torc::ServiceDiscovered)
                {
                    if (uuid == gLocalContext->GetUuid().toLatin1())
                    {
                        // this is us!
                    }
                    else if (m_serviceList.contains(uuid))
                    {
                        // register this as an additional source
                        QWriteLocker locker(&m_httpServiceLock);
                        for (int i = 0; i < m_discoveredServices.size(); ++i)
                            if (m_discoveredServices.at(i)->GetUuid() == uuid)
                                m_discoveredServices.at(i)->SetSource(TorcNetworkService::UPnP);
                    }
                    else
                    {
                        // need name, uuid, port, hosts, apiversion, priority, starttime, host?
                        QUrl location(event->Data().value("address").toString());
                        QString name = event->Data().value("name").toString();
                        bool secure = event->Data().contains("secure");
                        QList<QHostAddress> hosts;
                        hosts << QHostAddress(location.host());
                        TorcNetworkService *service = new TorcNetworkService(name, uuid, location.port(), secure, hosts);
                        service->SetAPIVersion(event->Data().value("apiversion").toString());
                        service->SetPriority(event->Data().value("priority").toInt());
                        service->SetStartTime(event->Data().value("starttime").toULongLong());
                        service->SetHost(location.host());
                        service->SetSource(TorcNetworkService::UPnP);
                        Add(service);
                        emit service->TryConnect();
                    }
                }
            }
        }
    }

    return QObject::event(Event);
}

/*! \brief Respond to a valid WebSocket upgrade request and schedule creation of a WebSocket on the give QTcpSocket
 *
 * \sa TorcWebSocket
 * \sa TorcHTTPServer::UpgradeSocket
*/
void TorcNetworkedContext::PeerConnected(TorcWebSocketThread* Thread, const QVariantMap & Data)
{
    if (!Thread)
        return;

    if (!gNetworkedContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "Upgrade request but no TorcNetworkedContext singleton");
        return;
    }

    // and create the WebSocket in the correct thread
    emit gNetworkedContext->NewPeer(Thread, Data);
}

/// \brief Pass Request to the remote connection identified by UUID
void TorcNetworkedContext::RemoteRequest(const QString &UUID, TorcRPCRequest *Request)
{
    if (!Request || UUID.isEmpty())
        return;

    if (!gNetworkedContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoteRequest but no TorcNetworkedContext singleton");
        return;
    }

    emit gNetworkedContext->NewRequest(UUID, Request);
}

/*! \brief Cancel Request associated with the connection identified by UUID
 *
 * See TorcWebSocket::CancelRequest for details on blocking.
 *
 * \sa TorcWebSocket::CancelRequest
*/
void TorcNetworkedContext::CancelRequest(const QString &UUID, TorcRPCRequest *Request, int Wait /* = 1000ms*/)
{
    if (!Request || UUID.isEmpty())
        return;

    if (!gNetworkedContext)
    {
        LOG(VB_GENERAL, LOG_ERR, "CancelRequest but no TorcNetworkedContext singleton");
        return;
    }

    if (Request && !Request->IsNotification())
    {
        Request->AddState(TorcRPCRequest::Cancelled);
        emit gNetworkedContext->RequestCancelled(UUID, Request);

        if (Wait > 0)
        {
            int count = 0;
            while (Request->IsShared() && (count++ < Wait))
                QThread::msleep(1);

            if (Request->IsShared())
                LOG(VB_GENERAL, LOG_ERR, "Request is still shared after cancellation");
        }
    }
}

void TorcNetworkedContext::HandleNewRequest(const QString &UUID, TorcRPCRequest *Request)
{
    if (!UUID.isEmpty() && m_serviceList.contains(UUID))
    {
        QReadLocker locker(&m_httpServiceLock);
        for (int i = 0; i < m_discoveredServices.size(); ++i)
        {
            if (m_discoveredServices[i]->GetUuid() == UUID)
            {
                m_discoveredServices[i]->RemoteRequest(Request);
                return;
            }
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, QString("Connection identified by '%1' unknown").arg(UUID));
}

void TorcNetworkedContext::HandleCancelRequest(const QString &UUID, TorcRPCRequest *Request)
{
    if (!UUID.isEmpty() && m_serviceList.contains(UUID))
    {
        QReadLocker locker(&m_httpServiceLock);
        for (int i = 0; i < m_discoveredServices.size(); ++i)
        {
            if (m_discoveredServices[i]->GetUuid() == UUID)
            {
                m_discoveredServices[i]->CancelRequest(Request);
                return;
            }
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, QString("Connection identified by '%1' unknown").arg(UUID));
}

void TorcNetworkedContext::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

void TorcNetworkedContext::HandleNewPeer(TorcWebSocketThread *Thread, const QVariantMap &Data)
{
    if (!Thread)
        return;

    QString UUID       = Data.value("uuid").toString();
    QString name       = Data.value("name").toString();
    int     port       = Data.value("port").toInt();
    QString apiversion = Data.value("apiversion").toString();
    int     priority   = Data.value("priority").toInt();
    qint64  starttime  = Data.value("starttime").toULongLong();
    QHostAddress address(Data.value("address").toString());

    if (UUID.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Received WebSocket for peer without UUID (%1) - closing").arg(name));
        Thread->quit();
        return;
    }

    if (m_serviceList.contains(UUID))
    {
        QReadLocker locker(&m_httpServiceLock);
        for (int i = 0; i < m_discoveredServices.size(); ++i)
        {
            if (m_discoveredServices[i]->GetUuid() == UUID && !TorcNetworkService::WeActAsServer(priority, starttime, UUID))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Received unexpected WebSocket from peer '%1' (%2) - closing")
                    .arg(m_discoveredServices[i]->GetName()).arg(UUID));
                Thread->quit();
                return;
            }
        }
    }

    TorcWebSocketThread* thread = TorcHTTPServer::TakeSocket(Thread);
    if (Thread == thread)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Received WebSocket for new peer '%1' (%2)").arg(name).arg(UUID));
        QList<QHostAddress> addresses;
        addresses << address;
        TorcNetworkService *service = new TorcNetworkService(name, UUID, port, Data.contains("secure"), addresses);
        service->SetWebSocketThread(thread);
        service->SetAPIVersion(apiversion);
        service->SetPriority(priority);
        service->SetStartTime(starttime);
        Add(service);
        return;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to take ownership of WebSocket from %1").arg(TorcNetwork::IPAddressToLiteral(address, port)));
}

void TorcNetworkedContext::Add(TorcNetworkService *Peer)
{
    if (Peer && !m_serviceList.contains(Peer->GetUuid()))
    {
        {
            QWriteLocker locker(&m_httpServiceLock);
            m_discoveredServices.append(Peer);
        }

        m_serviceList.append(Peer->GetUuid());
        emit PeersChanged();
        LOG(VB_GENERAL, LOG_INFO, QString("New Torc peer '%1' %2").arg(Peer->GetName()).arg(Peer->GetUuid()));
    }
}

void TorcNetworkedContext::Remove(const QString &UUID, TorcNetworkService::ServiceSource Source)
{
    if (m_serviceList.contains(UUID))
    {
        {
            QWriteLocker locker(&m_httpServiceLock);
            for (int i = 0; i < m_discoveredServices.size(); ++i)
            {
                if (m_discoveredServices.at(i)->GetUuid() == UUID)
                {
                    // remove the source first - this acts as a form of reference counting
                    m_discoveredServices.at(i)->RemoveSource(Source);

                    // don't delete if the service is still advertised by other means
                    if (m_discoveredServices.at(i)->GetSources() > TorcNetworkService::Spontaneous)
                        return;

                    // remove the item
                    m_discoveredServices.takeAt(i)->deleteLater();
                    break;
                }
            }
        }

        (void)m_serviceList.removeAll(UUID);
        emit PeersChanged();
        LOG(VB_GENERAL, LOG_INFO, QString("Torc peer %1 went away").arg(UUID));
    }
}

static class TorcNetworkedContextObject : public TorcAdminObject, public TorcStringFactory
{
  public:
    TorcNetworkedContextObject()
      : TorcAdminObject(TORC_ADMIN_HIGH_PRIORITY + 1 /* start after network and before http server */)
    {
        qRegisterMetaType<TorcRPCRequest*>();
    }

    ~TorcNetworkedContextObject()
    {
        Destroy();
    }

    void GetStrings(QVariantMap &Strings)
    {
        Strings.insert("NoPeers", QCoreApplication::translate("TorcNetworkedContext", "No other Torc devices detected"));
        // this is here to capture the plural translation
        (void)QCoreApplication::translate("TorcNetworkedContext", "%n other Torc device(s) discovered", nullptr, 2);
    }

    void Create(void)
    {
        Destroy();

        gNetworkedContext = new TorcNetworkedContext();
    }

    void Destroy(void)
    {
        delete gNetworkedContext;
        gNetworkedContext = nullptr;
    }

} TorcNetworkedContextObject;
