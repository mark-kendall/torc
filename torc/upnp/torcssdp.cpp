/* Class TorcSSDP
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

// Qt
#include <QHostAddress>

// Torc
#include "torclocalcontext.h"
#include "torchttpserver.h"
#include "torccoreutils.h"
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcupnp.h"
#include "torcssdp.h"

TorcSSDP* TorcSSDP::gSSDP        = NULL;
QMutex*   TorcSSDP::gSSDPLock    = new QMutex(QMutex::Recursive);
bool TorcSSDP::gSearchEnabled    = false;
bool TorcSSDP::gAnnounceEnabled  = false;
bool TorcSSDP::gAnnounceSecure   = false;

TorcSSDPSearchResponse::TorcSSDPSearchResponse(const QHostAddress &Address, const ResponseTypes Types, int Port)
  : m_responseAddress(Address),
    m_responseTypes(Types),
    m_port(Port)
{
}

TorcSSDPSearchResponse::TorcSSDPSearchResponse()
  : m_responseAddress(QHostAddress(QHostAddress::Any)),
    m_responseTypes(TorcSSDPSearchResponse::None),
    m_port(0)
{
}

/*! \class TorcSSDP
 *  \brief The public class for handling Simple Service Discovery Protocol searches and announcements
 *
 * All SSDP interaction is via the static methods Search, CancelSearch, Announce and CancelAnnounce
 *
 * \todo Revisit behaviour for search and announce wrt network availability and network allowed inbound/outbound
 * \todo Actually announce (with random 0-100ms delay) plus retry
 * \todo Schedule announce refreshes (half of cache-control)
 * \todo Respond to search requests
 * \todo Correct handling of multiple responses (i.e. via both IPv4 and IPv6)
*/

TorcSSDP::TorcSSDP()
  : QObject(),
    m_secure(false),
    m_serverString(),
    m_searching(false),
    m_firstSearchTimer(0),
    m_secondSearchTimer(0),
    m_announcing(false),
    m_firstAnnounceTimer(0),
    m_secondAnnounceTimer(0),
    m_refreshTimer(0),
    m_started(false),
    m_ipv4Address(),
    m_ipv6Address(),
    m_addressess(),
    m_ipv4GroupAddress(TORC_IPV4_UDP_MULTICAST_ADDR),
    m_ipv4MulticastSocket(NULL),
    m_ipv6LinkGroupBaseAddress(TORC_IPV6_UDP_MULTICAST_ADDR2),
    m_ipv6LinkMulticastSocket(NULL),
    m_discoveredDevices(),
    m_ipv4UnicastSocket(NULL),
    m_ipv6UnicastSocket(NULL),
    m_responseTimer(),
    m_responseQueue()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    m_serverString = QString("%1/%2 UPnP/1.0 Torc/0.1").arg(QSysInfo::kernelType()).arg(QSysInfo::kernelVersion());
#else
    m_serverString = QString("OldTorc/OldVersion UPnP/1.0 Torc/0.1");
#endif
    gLocalContext->AddObserver(this);

    if (TorcNetwork::IsAvailable())
        Start();

    // minimum advertisement duration is 30 mins (1800 seconds). Check for expiry
    // every 5 minutes and flush stale entries
    m_refreshTimer = startTimer(5 * 60 * 1000, Qt::VeryCoarseTimer);

    // start the response timer here and connect its signal
    // we use a full QTimer as we need to know how long it has remaining
    connect(&m_responseTimer, SIGNAL(timeout()), this, SLOT(ProcessResponses()));
    m_responseTimer.start(5000);
}

TorcSSDP::~TorcSSDP()
{
    Stop();

    if (m_refreshTimer)
        killTimer(m_refreshTimer);

    gLocalContext->RemoveObserver(this);
}

void TorcSSDP::Start(void)
{
    if (m_started)
        Stop();

    LOG(VB_GENERAL, LOG_INFO, QString("Starting SSDP as %1").arg(m_serverString));

    // try and get the interface the network is using
    QNetworkInterface interface;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    foreach (QNetworkInterface i, interfaces)
    {
        QNetworkInterface::InterfaceFlags flags = i.flags();
        if (flags & QNetworkInterface::IsUp &&
            flags & QNetworkInterface::IsRunning &&
            flags & QNetworkInterface::CanMulticast &&
            !(flags & QNetworkInterface::IsLoopBack))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Using interface '%1' for multicast").arg(i.name()));
            interface = i;
            break;
        }
    }

    // create the list of known own addresses
    m_ipv4Address = QString();
    m_ipv6Address = QString();
    m_addressess = QNetworkInterface::allAddresses();

    // try and determine suitable ipv4 and ipv6 addresses
    // at the moment I cannot test 'proper' IPv6 support, so allow link local for now
    QString ipv6linklocal;
    foreach (QHostAddress address, m_addressess)
    {
        if (TorcNetwork::IsGlobal(address))
            continue;
        if (!TorcNetwork::IsExternal(address))
            continue;
        if (TorcNetwork::IsLocal(address))
        {
            if (address.protocol() == QAbstractSocket::IPv4Protocol)
                m_ipv4Address = address.toString();
            else
                m_ipv6Address = address.toString();
        }
        else if (TorcNetwork::IsLinkLocal(address) && address.protocol() == QAbstractSocket::IPv6Protocol)
        {
            ipv6linklocal = address.toString();
        }
    }

    // fall back to link local for ipv6
    if (m_ipv6Address.isEmpty() && !ipv6linklocal.isEmpty())
        m_ipv6Address = ipv6linklocal;
    if (!m_ipv6Address.isEmpty() && !m_ipv6Address.startsWith("["))
        m_ipv6Address = QString("[%1]").arg(m_ipv6Address);

    if (m_ipv4Address.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, "Disabling SSDP IPv4 - no valid address detected");
    else
        LOG(VB_GENERAL, LOG_INFO, QString("SSDP IPv4 host url: %1").arg(m_ipv4Address));
    if (m_ipv6Address.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, "Disabling SSDP IPv6 - no valid address detected");
    else
        LOG(VB_GENERAL, LOG_INFO, QString("SSDP IPv6 host url: %1").arg(m_ipv6Address));

    // create sockets
    if (interface.isValid())
    {
        m_ipv4MulticastSocket       = CreateMulticastSocket(m_ipv4GroupAddress, this, interface);
        m_ipv6LinkMulticastSocket   = CreateMulticastSocket(m_ipv6LinkGroupBaseAddress, this, interface);
        m_ipv4UnicastSocket         = CreateSearchSocket(QHostAddress(QHostAddress::AnyIPv4), this);
        m_ipv6UnicastSocket         = CreateSearchSocket(QHostAddress(QHostAddress::AnyIPv6), this);
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, "Failed to create SSDP sockets - no suitable network interface");
    }

    // check whether search or announce was requested before we started
    bool search;
    bool announce;
    bool secure;
    {
        QMutexLocker locker(gSSDPLock);
        search   = gSearchEnabled;
        announce = gAnnounceEnabled;
        secure   = gAnnounceSecure;
    }

    if (search)
        StartSearch();

    if (announce)
        StartAnnounce(secure);

    m_started = true;
}

void TorcSSDP::Stop(void)
{
    // cancel search and announce
    StopSearch();
    StopAnnounce();

    // discard queued responses
    m_responseQueue.clear();

    // Network is no longer available - notify clients that services have gone away
    QHash<QString,TorcUPNPDescription>::const_iterator it2 = m_discoveredDevices.begin();
    for ( ; it2 != m_discoveredDevices.end(); ++it2)
    {
        QVariantMap data;
        data.insert("usn", it2.value().GetUSN());
        TorcEvent event = TorcEvent(Torc::ServiceWentAway, data);
        gLocalContext->Notify(event);
    }
    m_discoveredDevices.clear();

    m_addressess.clear();
    m_ipv4Address = QString();
    m_ipv6Address = QString();

    if (m_started)
        LOG(VB_GENERAL, LOG_INFO, "Stopping SSDP");
    m_started = false;

    if (m_ipv4MulticastSocket)
        m_ipv4MulticastSocket->close();
    if (m_ipv6LinkMulticastSocket)
        m_ipv6LinkMulticastSocket->close();
    if (m_ipv4UnicastSocket)
        m_ipv4UnicastSocket->close();
    if (m_ipv6UnicastSocket)
        m_ipv6UnicastSocket->close();

    delete m_ipv4MulticastSocket;
    delete m_ipv6LinkMulticastSocket;
    delete m_ipv4UnicastSocket;
    delete m_ipv6UnicastSocket;

    m_ipv4MulticastSocket     = NULL;
    m_ipv6LinkMulticastSocket = NULL;
    m_ipv4UnicastSocket       = NULL;
    m_ipv6UnicastSocket       = NULL;
}

/*! \fn    TorcSSDP::Search
 *  \brief Search for Torc UPnP device type
 *
 * \sa TorcSSDDP::CancelSearch
*/
void TorcSSDP::Search(void)
{
    QMutexLocker locker(gSSDPLock);
    gSearchEnabled = true;
    if (gSSDP)
        QMetaObject::invokeMethod(gSSDP, "SearchPriv", Qt::AutoConnection);
}

/*! \fn    TorcSSDP::CancelSearch
 *  \brief Stop searching for a UPnP device type
 *
 * Notifications about new or removed UPnP devices will no longer be sent.
*/
void TorcSSDP::CancelSearch(void)
{
    QMutexLocker locker(gSSDPLock);
    gSearchEnabled = false;
    if (gSSDP)
        QMetaObject::invokeMethod(gSSDP, "CancelSearchPriv", Qt::AutoConnection);
}

/*! \fn    TorcSSDP::Announce
 *  \brief Publish the device
*/
void TorcSSDP::Announce(bool Secure)
{
    QMutexLocker locker(gSSDPLock);
    gAnnounceEnabled = true;
    gAnnounceSecure  = Secure;
    if (gSSDP)
        QMetaObject::invokeMethod(gSSDP, "AnnouncePriv", Qt::AutoConnection, Q_ARG(bool, Secure));
}

/*! \fn    TorcSSDP::CancelAnnounce
*/
void TorcSSDP::CancelAnnounce(void)
{
    QMutexLocker locker(gSSDPLock);
    gAnnounceEnabled = false;
    gAnnounceSecure  = false;
    if (gSSDP)
        QMetaObject::invokeMethod(gSSDP, "CancelAnnouncePriv", Qt::AutoConnection);
}

TorcSSDP* TorcSSDP::Create(bool Destroy)
{
    QMutexLocker locker(gSSDPLock);

    if (!Destroy)
    {
        if (!gSSDP)
            gSSDP = new TorcSSDP();
        return gSSDP;
    }

    delete gSSDP;
    gSSDP = NULL;
    return NULL;
}

QUdpSocket* TorcSSDP::CreateSearchSocket(const QHostAddress &HostAddress, QObject *Owner)
{
    QUdpSocket *socket = new QUdpSocket();
    if (socket->bind(HostAddress, 1900, QUdpSocket::ShareAddress))
    {
        socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);
        socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
        QObject::connect(socket, SIGNAL(readyRead()), Owner, SLOT(Read()));
        LOG(VB_NETWORK, LOG_INFO, QString("%1 SSDP unicast socket %2:%3")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4")
            .arg(socket->localAddress().toString())
            .arg(QString::number(socket->localPort())));
        return socket;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind %1 SSDP search socket with address %3 (%2)")
        .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4").arg(socket->errorString())
        .arg(HostAddress.toString()));
    delete socket;
    return NULL;
}

QUdpSocket* TorcSSDP::CreateMulticastSocket(const QHostAddress &HostAddress, QObject *Owner, const QNetworkInterface &Interface)
{
    QUdpSocket *socket = new QUdpSocket();
    if (socket->bind(HostAddress, TORC_SSDP_UDP_MULTICAST_PORT, QUdpSocket::ShareAddress))
    {
        if (socket->joinMulticastGroup(HostAddress, Interface))
        {
            QObject::connect(socket, SIGNAL(readyRead()), Owner, SLOT(Read()));
            LOG(VB_NETWORK, LOG_INFO, QString("%1 SSDP multicast socket %2:%3")
                .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4")
                .arg(socket->localAddress().toString())
                .arg(QString::number(socket->localPort())));
            return socket;
        }

        LOG(VB_GENERAL, LOG_ERR, QString("Failed to join %1 multicast group (%2)")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4")
            .arg(socket->errorString()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to bind %1 SSDP multicast socket with address %3 (%2)")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? "IPv6" : "IPv4").arg(socket->errorString())
            .arg(HostAddress.toString()));
    }

    delete socket;
    return NULL;
}

void TorcSSDP::SendSearch(void)
{
    // evented when the network is available or we have restarted and then twice every five minutes
    // we only search for the Torc root device

    QString searchdevice = TORC_ROOT_UPNP_DEVICE;//"ssdp:all";
    static const QString search("M-SEARCH * HTTP/1.1\r\n"
                                "HOST: %1\r\n"
                                "MAN: \"ssdp:discover\"\r\n"
                                "MX: 3\r\n"
                                "ST: %2\r\n\r\n");

    if (!m_ipv4Address.isEmpty() && m_ipv4MulticastSocket && m_ipv4MulticastSocket->isValid() && m_ipv4MulticastSocket->state() == QAbstractSocket::BoundState)
    {
        m_ipv4MulticastSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 2);
        QByteArray ipv4search = QString(search).arg(TORC_IPV4_UDP_MULTICAST_URL).arg(searchdevice).toLocal8Bit();
        qint64 sent = m_ipv4MulticastSocket->writeDatagram(ipv4search, m_ipv4GroupAddress, TORC_SSDP_UDP_MULTICAST_PORT);
        if (sent != ipv4search.size())
            LOG(VB_GENERAL, LOG_ERR, QString("Error sending IPv4 search request (%1)").arg(m_ipv4MulticastSocket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, "Sent IPv4 SSDP global search request");
    }

    if (!m_ipv6Address.isEmpty() && m_ipv6LinkMulticastSocket && m_ipv6LinkMulticastSocket->isValid() && m_ipv6LinkMulticastSocket->state() == QAbstractSocket::BoundState)
    {
        m_ipv6LinkMulticastSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 2);
        QByteArray ipv6search = QString(search).arg(TORC_IPV6_UDP_MULTICAST_URL).arg(searchdevice).toLocal8Bit();
        qint64 sent = m_ipv6LinkMulticastSocket->writeDatagram(ipv6search, m_ipv6LinkGroupBaseAddress, TORC_SSDP_UDP_MULTICAST_PORT);
        if (sent != ipv6search.size())
            LOG(VB_GENERAL, LOG_ERR, QString("Error sending IPv6 search request (%1)").arg(m_ipv6LinkMulticastSocket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, "Sent IPv6 SSDP global search request");
    }
}

void TorcSSDP::SearchPriv(void)
{
    // if we haven't started or the network is unavailable, the search will be triggered when we restart
    if (m_started && TorcNetwork::IsAvailable())
        StartSearch();
}

void TorcSSDP::CancelSearchPriv(void)
{
    StopSearch();
}

void TorcSSDP::AnnouncePriv(bool Secure)
{
    // if we haven't started or the network is unavailable, announce will be triggered when we restart
    if (m_started && TorcNetwork::IsAvailable())
        StartAnnounce(Secure);
}

void TorcSSDP::CancelAnnouncePriv(void)
{
    StopAnnounce();
}

void TorcSSDP::StartAnnounce(bool Secure)
{
    m_secure = Secure;
    if (!m_announcing)
        LOG(VB_GENERAL, LOG_INFO, QString("Starting SSDP announce (%1)").arg(TORC_ROOT_UPNP_DEVICE));

    StopAnnounce();

    // schedule first announce almost immediately
    m_firstAnnounceTimer = startTimer(100, Qt::CoarseTimer);
    // and schedule another shortly afterwards
    m_secondAnnounceTimer = startTimer(500, Qt::CoarseTimer);

    m_announcing = true;
}

void TorcSSDP::StopAnnounce(void)
{
    if (m_announcing)
        LOG(VB_GENERAL, LOG_INFO, QString("Stopping SSDP announce (%1)").arg(TORC_ROOT_UPNP_DEVICE));

    if (m_firstAnnounceTimer)
    {
        killTimer(m_firstAnnounceTimer);
        m_firstAnnounceTimer = 0;
    }

    if (m_secondAnnounceTimer)
    {
        killTimer(m_secondAnnounceTimer);
        m_secondAnnounceTimer = 0;
    }

    // try and send out byebye
    if (m_announcing)
    {
        SendAnnounce(false, false); // IPv4 byebye
        SendAnnounce(true, false);  // IPv6 byebye
    }

    m_announcing = false;
}

void TorcSSDP::SendAnnounce(bool IPv6, bool Alive)
{
    QUdpSocket *socket = IPv6 ? m_ipv6LinkMulticastSocket : m_ipv4MulticastSocket;

    if ((IPv6 && m_ipv6Address.isEmpty()) || (!IPv6 && m_ipv4Address.isEmpty()))
        return;

    if (!socket || !socket->isValid() || socket->state() != QAbstractSocket::BoundState)
        return;

    /* We need to send out 3 packets and then repeat after a short period of time.
     * Packet 1 - NT: upnp:rootdevice                             USN: uuid:device-UUID::upnp:rootdevice
     * Packet 2 - NT: uuid:device-UUID                            USN: uuid:device-UUID
     * Packet 3 - NT: urn:schemas-torcapp-org:device:TorcClient:1 USN: uuid:device-UUID::urn:schemas-torcapp-org:device:TorcClient:1
    */

    static const QString notify("NOTIFY * HTTP/1.1\r\n"
                                "HOST: %1\r\n"
                                "CACHE-CONTROL: max-age=1800\r\n"
                                "LOCATION: http%2://%3:%4/upnp/description\r\n"
                                "NT: %5\r\n"
                                "NTS: ssdp:%6\r\n"
                                "SERVER: %7\r\n"
                                "USN: %8\r\n"
                                "NAME: %9\r\n"
                                "APIVERSION: %10\r\n"
                                "STARTTIME: %11\r\n"
                                "PRIORITY: %12\r\n%13\r\n");

    QString ip           = IPv6 ? TORC_IPV6_UDP_MULTICAST_URL : TORC_IPV4_UDP_MULTICAST_URL;
    QHostAddress address = IPv6 ? m_ipv6LinkGroupBaseAddress : m_ipv4GroupAddress;
    QString uuid         = QString("uuid:") + gLocalContext->GetUuid();
    QString alive        = Alive ? "alive" : "byebye";
    QString url          = IPv6 ? m_ipv6Address : m_ipv4Address;
    int port             = TorcHTTPServer::GetPort();
    QString secure       = m_secure ? "s" : "";
    QString secure2      = m_secure ? "SECURE: yes\r\n" : "";
    QString name         = TorcHTTPServer::ServerDescription();
    QString apiversion   = TorcHTTPServices::GetVersion();
    QString starttime    = QString::number(gLocalContext->GetStartTime());
    QString priority     = QString::number(gLocalContext->GetPriority());
    QByteArray packet1   = QString(notify).arg(ip).arg(secure).arg(url).arg(port).arg("upnp:rootdevice").arg(alive).arg(m_serverString).arg(uuid + "::upnp::rootdevice")
                                          .arg(name).arg(apiversion).arg(starttime).arg(priority).arg(secure2).toLocal8Bit();
    QByteArray packet2   = QString(notify).arg(ip).arg(secure).arg(url).arg(port).arg(uuid).arg(alive).arg(m_serverString).arg(uuid)
                                          .arg(name).arg(apiversion).arg(starttime).arg(priority).arg(secure2).toLocal8Bit();
    QByteArray packet3   = QString(notify).arg(ip).arg(secure).arg(url).arg(port).arg(TORC_ROOT_UPNP_DEVICE).arg(alive).arg(m_serverString).arg(uuid + "::" + TORC_ROOT_UPNP_DEVICE)
                                          .arg(name).arg(apiversion).arg(starttime).arg(priority).arg(secure2).toLocal8Bit();

    socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);
    qint64 sent = socket->writeDatagram(packet1, address, TORC_SSDP_UDP_MULTICAST_PORT);
    sent       += socket->writeDatagram(packet2, address, TORC_SSDP_UDP_MULTICAST_PORT);
    sent       += socket->writeDatagram(packet3, address, TORC_SSDP_UDP_MULTICAST_PORT);
    if (sent != (packet1.size() + packet2.size() + packet3.size()))
        LOG(VB_GENERAL, LOG_ERR, QString("Error sending 3 %1 SSDP NOTIFYs (%1)").arg(IPv6 ? "IPv6" : "IPv4").arg(m_ipv4MulticastSocket->errorString()));
    else
        LOG(VB_NETWORK, LOG_INFO, QString("Sent 3 %1 SSDP NOTIFYs ").arg(IPv6 ? "IPv6" : "IPv4"));
}

bool TorcSSDP::event(QEvent *Event)
{
    if (Event->type() == TorcEvent::TorcEventType)
    {
        TorcEvent* event = dynamic_cast<TorcEvent*>(Event);
        if (event)
        {
            if (event->GetEvent() == Torc::NetworkAvailable)
            {
                Start();
            }
            else if (event->GetEvent() == Torc::NetworkUnavailable)
            {
                Stop();
            }
        }
    }
    else if (Event->type() == QEvent::Timer)
    {
        QTimerEvent* event = dynamic_cast<QTimerEvent*>(Event);
        if (event)
        {
            // N.B. we kill the timers as we don't know whether they were the initial, short
            // duration timers or not

            // 2 search timers will be active if enabled, 5 seconds apart.
            // reschedule them for 5 minutes
            if (event->timerId() == m_firstSearchTimer)
            {
                killTimer(m_firstSearchTimer);
                SendSearch();
                m_firstSearchTimer = startTimer(5 * 60 * 1000);
            }
            else if (event->timerId() == m_secondSearchTimer)
            {
                killTimer(m_secondSearchTimer);
                SendSearch();
                m_secondSearchTimer = startTimer(5 * 60 * 1000);
            }
            // 2 announce timers will be active if enabled - a short time apart
            // reshechedule them for every 10 minutes - although max-age is set to 30 mins (1800 seconds)
            else if (event->timerId() == m_firstAnnounceTimer)
            {
                killTimer(m_firstAnnounceTimer);
                SendAnnounce(false, true); //IPv4 alive
                SendAnnounce(true, true);  //IPv6 alive
                m_firstAnnounceTimer = startTimer(10 * 60 * 1000);
            }
            else if (event->timerId() == m_secondAnnounceTimer)
            {
                killTimer(m_secondAnnounceTimer);
                SendAnnounce(false, true);
                SendAnnounce(true, true);
                m_secondAnnounceTimer = startTimer(10 * 60 * 1000);
            }
            // refresh the cache
            else if (event->timerId() == m_refreshTimer)
            {
                Refresh();
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, "Timer even from unknown timer");
            }
        }
    }

    return QObject::event(Event);
}

void TorcSSDP::Read(void)
{
    QUdpSocket *socket = dynamic_cast<QUdpSocket*>(sender());
    while (socket && socket->hasPendingDatagrams())
    {
        QHostAddress address;
        quint16 port;
        QByteArray datagram;
        datagram.resize(socket->pendingDatagramSize());
        socket->readDatagram(datagram.data(), datagram.size(), &address, &port);

        // filter out our own messages.
        // N.B. this also filters out messages from other instances running on the same device (with the same ip)
        if ((m_ipv4MulticastSocket && (port == m_ipv4MulticastSocket->localPort())) || (m_ipv6LinkMulticastSocket && (port == m_ipv6LinkMulticastSocket->localPort())))
            if (m_addressess.contains(address))
                continue;

        // use a QString for greater flexibility in splitting and searching text
        QString data(datagram);
        LOG(VB_NETWORK, LOG_DEBUG, "Raw datagram:\r\n" + data);

        // split data into lines
        QStringList lines = data.split("\r\n", QString::SkipEmptyParts);
        if (lines.isEmpty())
            continue;

        QMap<QString,QString> headers;

        // pull out type
        QString type = lines.takeFirst().trimmed();

        // process headers
        while (!lines.isEmpty())
        {
            QString header = lines.takeFirst();
            int index = header.indexOf(':');
            if (index > -1)
                headers.insert(header.left(index).trimmed().toLower(), header.mid(index + 1).trimmed());
        }

        if (type.startsWith("HTTP", Qt::CaseInsensitive))
        {
            // response
            if (headers.contains("cache-control"))
            {
                qint64 expires = GetExpiryTime(headers.value("cache-control"));
                if (expires > 0)
                    ProcessDevice(headers, expires, true/*add*/);
            }
        }
        else if (type.startsWith("NOTIFY", Qt::CaseInsensitive))
        {
            // notfification ssdp:alive or ssdp:bye
            if (headers.contains("nts"))
            {
                bool add = headers["nts"] != "ssdp:byebye";

                if (add)
                {
                    if (headers.contains("cache-control"))
                    {
                        qint64 expires = GetExpiryTime(headers.value("cache-control"));
                        if (expires > 0)
                            ProcessDevice(headers, expires, true/*add*/);
                    }
                }
                else
                {
                    ProcessDevice(headers, 1, false/*remove*/);
                }
            }
        }
        else if (type.startsWith("M-SEARCH", Qt::CaseInsensitive))
        {
            // check MAN for ssdp:discover
            if (headers.value("man").contains("ssdp:discover",Qt::CaseInsensitive))
            {
                // already have host address from datagram
                // need MX value in seconds
                // lazy - should be present and in the range 1-120 - but limit to 5
                int mx = qBound(1, headers.value("mx").toInt(), 5);

                // check the target
                TorcSSDPSearchResponse::ResponseTypes types(TorcSSDPSearchResponse::None);
                QString st = headers.value("st");
                if (st == "ssdp:all")
                {
                    types |= TorcSSDPSearchResponse::Root;
                    types |= TorcSSDPSearchResponse::UUID;
                    types |= TorcSSDPSearchResponse::Device;
                }
                else if (st == "upnp:rootdevice")
                {
                    types |= TorcSSDPSearchResponse::Root;
                }
                else if (st.startsWith("uuid:"))
                {
                    if (st.mid(5) == TORC_ROOT_UPNP_DEVICE)
                        types |= TorcSSDPSearchResponse::UUID;
                }
                else if (st == TORC_ROOT_UPNP_DEVICE)
                {
                    types |= TorcSSDPSearchResponse::Device;
                }

                if (types > TorcSSDPSearchResponse::None)
                {
                    qint64 firstrandom  = qrand() % ((mx * 1000) >> 1);
                    qint64 secondrandom = qrand() % (mx * 1000);
                    m_responseQueue.insertMulti(QDateTime::currentMSecsSinceEpoch() + firstrandom, TorcSSDPSearchResponse(address, types, port));
                    m_responseQueue.insertMulti(QDateTime::currentMSecsSinceEpoch() + secondrandom, TorcSSDPSearchResponse(address, types, port));

                    // if the first response is due before the response timer times out, reset it.
                    firstrandom = qMin(firstrandom, secondrandom);
                    if (firstrandom < m_responseTimer.remainingTime())
                        m_responseTimer.start(firstrandom + 1);
                }
            }
            else
            {
                LOG(VB_NETWORK, LOG_INFO, "Unknown MAN value in search request ");
            }
        }
    }
}

void TorcSSDP::ProcessResponses(void)
{
    qint64 timenow = QDateTime::currentMSecsSinceEpoch();

    bool finished = false;
    while (!finished && !m_responseQueue.isEmpty())
    {
        qint64 time = m_responseQueue.firstKey();
        if (time <= timenow)
        {
            const TorcSSDPSearchResponse response = m_responseQueue.take(time);
            ProcessResponse(response);
        }
        else
        {
            finished = true;
        }
    }

    qint64 nextcheck = 5000;
    if (!m_responseQueue.isEmpty())
        nextcheck = m_responseQueue.firstKey() - timenow;
    m_responseTimer.start(nextcheck);
}

void TorcSSDP::ProcessResponse(const TorcSSDPSearchResponse &Response)
{
    /* As per notify (NT -> ST)
     * Root   - ST: upnp:rootdevice                             USN: uuid:device-UUID::upnp:rootdevice
     * UUID   - ST: uuid:device-UUID                            USN: uuid:device-UUID
     * Device - ST: urn:schemas-torcapp-org:device:TorcClient:1 USN: uuid:device-UUID::urn:schemas-torcapp-org:device:TorcClient:1
    */

    // TODO this duplicates format from TorcHTTPRequest
    static const QString dateformat("ddd, dd MMM yyyy HH:mm:ss 'GMT'");
    static const QString reply("HTTP/1.1 200 OK\r\n"
                               "CACHE-CONTROL: max-age=1800\r\n"
                               "DATE: %1\r\n"
                               "EXT:\r\n"
                               "LOCATION: http%2://%3:%4/upnp/description\r\n"
                               "SERVER: %5\r\n"
                               "ST: %6\r\n"
                               "USN: %7\r\n"
                               "NAME: %8\r\n"
                               "APIVERSION: %9\r\n"
                               "STARTTIME: %10\r\n"
                               "PRIORITY: %11\r\n%12\r\n");

    QString date       = QDateTime::currentDateTime().toString(dateformat);
    QString uuid       = QString("uuid:") + gLocalContext->GetUuid();
    bool ipv6          = Response.m_responseAddress.protocol() == QAbstractSocket::IPv6Protocol;
    if ((ipv6 && m_ipv6Address.isEmpty()) || (!ipv6 && m_ipv4Address.isEmpty()))
        return;
    QUdpSocket *socket = ipv6 ? m_ipv6UnicastSocket : m_ipv4UnicastSocket;
    int port           = TorcHTTPServer::GetPort();
    QString raddress   = QString("%1:%2").arg(Response.m_responseAddress.toString()).arg(Response.m_port);
    QString secure     = m_secure ? "s" : "";
    QString secure2    = m_secure ? "SECURE: yes\r\n" : "";
    QString name       = TorcHTTPServer::ServerDescription();
    QString apiversion = TorcHTTPServices::GetVersion();
    QString starttime  = QString::number(gLocalContext->GetStartTime());
    QString priority   = QString::number(gLocalContext->GetPriority());
    int sent;
    QByteArray packet;
    if (Response.m_responseTypes.testFlag(TorcSSDPSearchResponse::Root))
    {
        packet = QString(reply).arg(date).arg(secure).arg(ipv6 ? m_ipv6Address : m_ipv4Address).arg(port).arg(m_serverString).arg("upnp:rootdevice")
                               .arg(uuid + "::upnp:rootdevice").arg(name).arg(apiversion).arg(starttime).arg(priority).arg(secure2).toLocal8Bit();
        sent = socket->writeDatagram(packet, Response.m_responseAddress, Response.m_port);
        if (sent != packet.size())
            LOG(VB_GENERAL, LOG_WARNING, QString("Error sending SSDP root search response to %1 (%2)").arg(raddress).arg(socket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, QString("Sent SSDP root search response to %1").arg(raddress));
    }

    if (Response.m_responseTypes.testFlag(TorcSSDPSearchResponse::UUID))
    {
        packet = QString(reply).arg(date).arg(secure).arg(ipv6 ? m_ipv6Address : m_ipv4Address).arg(port).arg(m_serverString).arg(uuid)
                               .arg(uuid).arg(name).arg(apiversion).arg(starttime).arg(priority).arg(secure2).toLocal8Bit();
        sent = socket->writeDatagram(packet, Response.m_responseAddress, Response.m_port);
        if (sent != packet.size())
            LOG(VB_GENERAL, LOG_WARNING, QString("Error sending SSDP UUID search response to %1 (%2)").arg(raddress).arg(socket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, QString("Sent SSDP UUID search response to %1").arg(raddress));
    }

    if (Response.m_responseTypes.testFlag(TorcSSDPSearchResponse::Device))
    {
        packet = QString(reply).arg(date).arg(secure).arg(ipv6 ? m_ipv6Address : m_ipv4Address).arg(port).arg(m_serverString).arg(TORC_ROOT_UPNP_DEVICE)
                               .arg(uuid + "::" + TORC_ROOT_UPNP_DEVICE).arg(name).arg(apiversion).arg(starttime).arg(priority).arg(secure2).toLocal8Bit();
        sent = socket->writeDatagram(packet, Response.m_responseAddress, Response.m_port);
        if (sent != packet.size())
            LOG(VB_GENERAL, LOG_WARNING, QString("Error sending SSDP Device search response to %1 (%2)").arg(raddress).arg(socket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, QString("Sent SSDP UUID Device response to %1").arg(raddress));
    }
}

void TorcSSDP::Refresh(void)
{
    if (!m_started)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // remove stale discovered devices (if still present, they should have notified
    // a refresh)
    QList<TorcUPNPDescription> removed;
    QMutableHashIterator<QString,TorcUPNPDescription> it(m_discoveredDevices);
    while (it.hasNext())
    {
        it.next();
        if (it.value().GetExpiry() < now)
        {
            removed << it.value();
            it.remove();
        }
    }

    // notify removal
    QList<TorcUPNPDescription>::const_iterator it2 = removed.begin();
    for ( ; it2 != removed.end(); ++it2)
    {
        QVariantMap data;
        data.insert("usn", (*it2).GetUSN());
        TorcEvent event = TorcEvent(Torc::ServiceWentAway, data);
        gLocalContext->Notify(event);
    }


    LOG(VB_NETWORK, LOG_INFO, QString("Removed %1 stale cache entries").arg(removed.size()));
}

void TorcSSDP::ProcessDevice(const QMap<QString,QString> &Headers, qint64 Expires, bool Add)
{
    if (!Headers.contains("usn") || !Headers.contains("location") || Expires < 1)
        return;

    QString USN = Headers.value("usn");
    QString location = Headers.value("location");

    if (!USN.contains(TORC_ROOT_UPNP_DEVICE))
        return;

    bool notify = true;

    if (m_discoveredDevices.contains(USN))
    {
        // is this just an expiry update?
        if (Add)
        {
            notify = false;
            (void)m_discoveredDevices.remove(USN);
            m_discoveredDevices.insert(USN, TorcUPNPDescription(USN, location, Expires));
        }
        else
        {
            (void)m_discoveredDevices.remove(USN);
        }
    }
    else if (Add)
    {
        m_discoveredDevices.insert(USN, TorcUPNPDescription(USN, location, Expires));
    }

    // update interested parties
    if (notify)
    {
        QVariantMap data;
        data.insert("usn", USN);
        data.insert("address", location);
        data.insert("name", Headers.value("name"));
        data.insert("apiversion", Headers.value("apiversion"));
        data.insert("starttime", Headers.value("starttime"));
        data.insert("priority", Headers.value("priority"));
        if (Headers.contains("secure"))
            data.insert("secure", "yes");
        TorcEvent event(Add ? Torc::ServiceDiscovered : Torc::ServiceWentAway, data);
        gLocalContext->Notify(event);
    }
}

void TorcSSDP::StartSearch(void)
{
    if (!m_searching)
        LOG(VB_GENERAL, LOG_INFO, QString("Starting SSDP search for %1 devices").arg(TORC_ROOT_UPNP_DEVICE));

    StopSearch();

    // schedule first search almost immediately - this is evented to ensure subsequent searches are scheduled
    m_firstSearchTimer = startTimer(1, Qt::CoarseTimer);
    // and schedule another search for after the MX time in our first request (3)
    m_secondSearchTimer = startTimer(5000, Qt::CoarseTimer);

    m_searching = true;
}

void TorcSSDP::StopSearch(void)
{
    if (m_searching)
        LOG(VB_GENERAL, LOG_INFO, QString("Stopping SSDP search for %1 devices").arg(TORC_ROOT_UPNP_DEVICE));

    if (m_firstSearchTimer)
    {
        killTimer(m_firstSearchTimer);
        m_firstSearchTimer = 0;
    }

    if (m_secondSearchTimer)
    {
        killTimer(m_secondSearchTimer);
        m_secondSearchTimer = 0;
    }

    m_searching = false;
}

qint64 TorcSSDP::GetExpiryTime(const QString &Expires)
{
    // NB this ignores the date header - just assume it is correct
    int index = Expires.indexOf("max-age", 0, Qt::CaseInsensitive);
    if (index > -1)
    {
        int index2 = Expires.indexOf("=", index);
        if (index2 > -1)
        {
            int seconds = Expires.mid(index2 + 1).toInt();
            return QDateTime::currentMSecsSinceEpoch() + 1000 * seconds;
        }
    }

    return -1;
}

TorcSSDPThread::TorcSSDPThread() : TorcQThread("SSDP")
{
}

void TorcSSDPThread::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, "SSDP thread starting");
    TorcSSDP::Create();
}

void TorcSSDPThread::Finish(void)
{
    TorcSSDP::Create(true/*destroy*/);
    LOG(VB_GENERAL, LOG_INFO, "SSDP thread stopping");
}

class TorcSSDPObject : public TorcAdminObject
{
  public:
    TorcSSDPObject()
      : TorcAdminObject(TORC_ADMIN_CRIT_PRIORITY - 1), // start after network but before TorcNetworkedContext
        m_ssdpThread(NULL)
    {
        qRegisterMetaType<TorcUPNPDescription>();
    }

    ~TorcSSDPObject()
    {
        Destroy();
    }

    void Create(void)
    {
        Destroy();

        m_ssdpThread = new TorcSSDPThread();
        m_ssdpThread->start();
    }

    void Destroy(void)
    {
        if (m_ssdpThread)
        {
            m_ssdpThread->quit();
            m_ssdpThread->wait();
            delete m_ssdpThread;
        }

        m_ssdpThread = NULL;
    }

  private:
    Q_DISABLE_COPY(TorcSSDPObject)
    TorcSSDPThread *m_ssdpThread;

} TorcSSDPObject;
