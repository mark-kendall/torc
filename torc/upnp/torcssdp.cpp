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

TorcSSDP* TorcSSDP::gSSDP        = nullptr;
QMutex*   TorcSSDP::gSSDPLock    = new QMutex(QMutex::Recursive);
bool TorcSSDP::gSearchEnabled    = false;
TorcHTTPServer::Status TorcSSDP::gSearchOptions = TorcHTTPServer::Status();
bool TorcSSDP::gAnnounceEnabled  = false;
TorcHTTPServer::Status TorcSSDP::gAnnounceOptions = TorcHTTPServer::Status();

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
*/

TorcSSDP::TorcSSDP()
  : QObject(),
    m_searchOptions(),
    m_announceOptions(),
    m_serverString(),
    m_searching(false),
    m_searchDebugged(false),
    m_firstSearchTimer(0),
    m_secondSearchTimer(0),
    m_firstAnnounceTimer(0),
    m_secondAnnounceTimer(0),
    m_refreshTimer(0),
    m_started(false),
    m_ipv4Address(),
    m_ipv6Address(),
    m_addressess(),
    m_ipv4GroupAddress(TORC_IPV4_UDP_MULTICAST_ADDR),
    m_ipv4MulticastSocket(nullptr),
    m_ipv6LinkGroupBaseAddress(TORC_IPV6_UDP_MULTICAST_ADDR2),
    m_ipv6LinkMulticastSocket(nullptr),
    m_discoveredDevices(),
    m_ipv4UnicastSocket(nullptr),
    m_ipv6UnicastSocket(nullptr),
    m_responseTimer(),
    m_responseQueue()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    m_serverString = QStringLiteral("%1/%2 UPnP/1.0 Torc/0.1").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
#else
    m_serverString = QStringLiteral("OldTorc/OldVersion UPnP/1.0 Torc/0.1");
#endif
    gLocalContext->AddObserver(this);

    if (TorcNetwork::IsAvailable())
        Start();

    // minimum advertisement duration is 30 mins (1800 seconds). Check for expiry
    // every 5 minutes and flush stale entries
    m_refreshTimer = startTimer(5 * 60 * 1000, Qt::VeryCoarseTimer);

    // start the response timer here and connect its signal
    // we use a full QTimer as we need to know how long it has remaining
    connect(&m_responseTimer, &QTimer::timeout, this, &TorcSSDP::ProcessResponses);
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

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Starting SSDP as %1").arg(m_serverString));

    // try and get the interface the network is using
    QNetworkInterface interface;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    foreach (const QNetworkInterface &i, interfaces)
    {
        QNetworkInterface::InterfaceFlags flags = i.flags();
        if (flags & QNetworkInterface::IsUp &&
            flags & QNetworkInterface::IsRunning &&
            flags & QNetworkInterface::CanMulticast &&
            !(flags & QNetworkInterface::IsLoopBack))
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Using interface '%1' for multicast").arg(i.name()));
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
    if (!m_ipv6Address.isEmpty() && !m_ipv6Address.startsWith('['))
        m_ipv6Address = QStringLiteral("[%1]").arg(m_ipv6Address);

    if (m_ipv4Address.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Disabling SSDP IPv4 - no valid address detected"));
    else
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSDP IPv4 host url: %1").arg(m_ipv4Address));
    if (m_ipv6Address.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Disabling SSDP IPv6 - no valid address detected"));
    else
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSDP IPv6 host url: %1 (if enabled)").arg(m_ipv6Address));

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
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Failed to create SSDP sockets - no suitable network interface"));
    }

    // check whether search or announce was requested before we started
    bool search;
    bool announce;
    {
        QMutexLocker locker(gSSDPLock);
        search    = gSearchEnabled;
        announce  = gAnnounceEnabled;
    }

    if (search)
        StartSearch();

    if (announce)
        StartAnnounce();

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
    QHash<QString,TorcUPNPDescription>::const_iterator it2 = m_discoveredDevices.constBegin();
    for ( ; it2 != m_discoveredDevices.constEnd(); ++it2)
    {
        QVariantMap data;
        data.insert(QStringLiteral("usn"), it2.value().GetUSN());
        TorcEvent event(Torc::ServiceWentAway, data);
        gLocalContext->Notify(event);
    }
    m_discoveredDevices.clear();

    m_addressess.clear();
    m_ipv4Address = QString();
    m_ipv6Address = QString();

    if (m_started)
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Stopping SSDP"));
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

    m_ipv4MulticastSocket     = nullptr;
    m_ipv6LinkMulticastSocket = nullptr;
    m_ipv4UnicastSocket       = nullptr;
    m_ipv6UnicastSocket       = nullptr;
}

/*! \fn    TorcSSDP::Search
 *  \brief Search for Torc UPnP device type
 *
 * \sa TorcSSDDP::CancelSearch
*/
void TorcSSDP::Search(TorcHTTPServer::Status Options)
{
    QMutexLocker locker(gSSDPLock);
    gSearchEnabled = true;
    gSearchOptions = Options;
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
void TorcSSDP::Announce(TorcHTTPServer::Status Options)
{
    QMutexLocker locker(gSSDPLock);
    gAnnounceEnabled = true;
    gAnnounceOptions = Options;
    if (gSSDP)
        QMetaObject::invokeMethod(gSSDP, "AnnouncePriv", Qt::AutoConnection);
}

/*! \fn    TorcSSDP::CancelAnnounce
*/
void TorcSSDP::CancelAnnounce(void)
{
    QMutexLocker locker(gSSDPLock);
    gAnnounceEnabled = false;
    gAnnounceOptions.port = 0;
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
    gSSDP = nullptr;
    return nullptr;
}

#define IPv4 QStringLiteral("IPv4")
#define IPv6 QStringLiteral("IPv6")
QUdpSocket* TorcSSDP::CreateSearchSocket(const QHostAddress &HostAddress, TorcSSDP *Owner)
{
    QUdpSocket *socket = new QUdpSocket();
    if (socket->bind(HostAddress, 1900, QUdpSocket::ShareAddress))
    {
        socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);
        socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
        QObject::connect(socket, &QUdpSocket::readyRead, Owner, &TorcSSDP::Read);
        LOG(VB_NETWORK, LOG_INFO, QStringLiteral("%1 SSDP unicast socket %2:%3")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? IPv6 : IPv4,
                 socket->localAddress().toString(), QString::number(socket->localPort())));
        return socket;
    }

    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to bind %1 SSDP search socket with address %3 (%2)")
        .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? IPv6 : IPv4, socket->errorString(), HostAddress.toString()));
    delete socket;
    return nullptr;
}

QUdpSocket* TorcSSDP::CreateMulticastSocket(const QHostAddress &HostAddress, TorcSSDP *Owner, const QNetworkInterface &Interface)
{
    QUdpSocket *socket = new QUdpSocket();
    if (socket->bind(HostAddress, TORC_SSDP_UDP_MULTICAST_PORT, QUdpSocket::ShareAddress))
    {
        if (socket->joinMulticastGroup(HostAddress, Interface))
        {
            QObject::connect(socket, &QUdpSocket::readyRead, Owner, &TorcSSDP::Read);
            LOG(VB_NETWORK, LOG_INFO, QStringLiteral("%1 SSDP multicast socket %2:%3")
                .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? IPv6 : IPv4,
                     socket->localAddress().toString(),
                     QString::number(socket->localPort())));
            return socket;
        }

        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to join %1 multicast group (%2)")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? IPv6 : IPv4, socket->errorString()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to bind %1 SSDP multicast socket with address %3 (%2)")
            .arg(HostAddress.protocol() == QAbstractSocket::IPv6Protocol ? IPv6 : IPv4, socket->errorString(), HostAddress.toString()));
    }

    delete socket;
    return nullptr;
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
        QByteArray ipv4search = QString(search).arg(TORC_IPV4_UDP_MULTICAST_URL, searchdevice).toLocal8Bit();
        qint64 sent = m_ipv4MulticastSocket->writeDatagram(ipv4search, m_ipv4GroupAddress, TORC_SSDP_UDP_MULTICAST_PORT);
        if (sent != ipv4search.size())
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending IPv4 search request (%1)").arg(m_ipv4MulticastSocket->errorString()));
        }
        else if (!m_searchDebugged)
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sent IPv4 SSDP global search request"));
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Will send further IPv4 requests every 5 minutes"));
        }
    }

    if (m_searchOptions.ipv6 && !m_ipv6Address.isEmpty() && m_ipv6LinkMulticastSocket && m_ipv6LinkMulticastSocket->isValid() && m_ipv6LinkMulticastSocket->state() == QAbstractSocket::BoundState)
    {
        m_ipv6LinkMulticastSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 2);
        QByteArray ipv6search = QString(search).arg(TORC_IPV6_UDP_MULTICAST_URL, searchdevice).toLocal8Bit();
        qint64 sent = m_ipv6LinkMulticastSocket->writeDatagram(ipv6search, m_ipv6LinkGroupBaseAddress, TORC_SSDP_UDP_MULTICAST_PORT);
        if (sent != ipv6search.size())
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending IPv6 search request (%1)").arg(m_ipv6LinkMulticastSocket->errorString()));
        }
        else if (!m_searchDebugged)
        {
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sent IPv6 SSDP global search request"));
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Will send further IPv6 requests every 5 minutes"));
        }
    }
    m_searchDebugged = true;
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

void TorcSSDP::AnnouncePriv(void)
{
    // if we haven't started or the network is unavailable, announce will be triggered when we restart
    if (m_started && TorcNetwork::IsAvailable())
        StartAnnounce();
}

void TorcSSDP::CancelAnnouncePriv(void)
{
    StopAnnounce();
}

void TorcSSDP::StartAnnounce(void)
{
    gSSDPLock->lock();
    TorcHTTPServer::Status newoptions = gAnnounceOptions;
    gSSDPLock->unlock();

    if (m_announceOptions == newoptions)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSDP announce options unchanged - not restarting"));
        return;
    }

    StopAnnounce();
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Starting SSDP announce (%1)").arg(TORC_ROOT_UPNP_DEVICE));
    m_announceOptions = newoptions;

    // schedule first announce almost immediately
    m_firstAnnounceTimer = startTimer(100, Qt::CoarseTimer);
    // and schedule another shortly afterwards
    m_secondAnnounceTimer = startTimer(500, Qt::CoarseTimer);

}

void TorcSSDP::StopAnnounce(void)
{
    if (m_announceOptions.port)
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Stopping SSDP announce (%1)").arg(TORC_ROOT_UPNP_DEVICE));

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
    if (m_announceOptions.port)
    {
        SendAnnounce(false, false); // IPv4 byebye
        if (m_announceOptions.ipv6)
            SendAnnounce(true, false);  // IPv6 byebye
    }

    m_announceOptions.port = 0;
}

void TorcSSDP::SendAnnounce(bool IsIPv6, bool Alive)
{
    if (!m_announceOptions.port)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot send announce - no port set"));
        return;
    }

    QUdpSocket *socket = IsIPv6 ? m_ipv6LinkMulticastSocket : m_ipv4MulticastSocket;

    if ((IsIPv6 && m_ipv6Address.isEmpty()) || (!IsIPv6 && m_ipv4Address.isEmpty()))
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

    QString ip           = IsIPv6 ? TORC_IPV6_UDP_MULTICAST_URL : TORC_IPV4_UDP_MULTICAST_URL;
    QHostAddress address = IsIPv6 ? m_ipv6LinkGroupBaseAddress : m_ipv4GroupAddress;
    QString uuid         = QStringLiteral("uuid:") + gLocalContext->GetUuid();
    QString alive        = Alive ? QStringLiteral("alive") :QStringLiteral( "byebye");
    QString url          = IsIPv6 ? m_ipv6Address : m_ipv4Address;
    QString secure       = m_announceOptions.secure ? QStringLiteral("s") : QStringLiteral("");
    QString secure2      = m_announceOptions.secure ? QStringLiteral("SECURE: yes\r\n") : QStringLiteral("");
    QString name         = TorcHTTPServer::ServerDescription();
    QString apiversion   = TorcHTTPServices::GetVersion();
    QString starttime    = QString::number(gLocalContext->GetStartTime());
    QString priority     = QString::number(gLocalContext->GetPriority());
    QByteArray packet1   = QString(notify).arg(ip, secure, url).arg(m_announceOptions.port)
                                          .arg(QStringLiteral("upnp:rootdevice"), alive, m_serverString, QStringLiteral("%1::upnp::rootdevice").arg(uuid), name, apiversion, starttime, priority, secure2).toLocal8Bit();
    QByteArray packet2   = QString(notify).arg(ip, secure, url).arg(m_announceOptions.port)
                                          .arg(uuid, alive, m_serverString, uuid, name, apiversion, starttime, priority, secure2).toLocal8Bit();
    QByteArray packet3   = QString(notify).arg(ip, secure, url).arg(m_announceOptions.port)
                                          .arg(TORC_ROOT_UPNP_DEVICE, alive, m_serverString, QStringLiteral("%1::%2").arg(uuid , TORC_ROOT_UPNP_DEVICE), name, apiversion, starttime, priority, secure2).toLocal8Bit();

    socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 4);
    qint64 sent = socket->writeDatagram(packet1, address, TORC_SSDP_UDP_MULTICAST_PORT);
    sent       += socket->writeDatagram(packet2, address, TORC_SSDP_UDP_MULTICAST_PORT);
    sent       += socket->writeDatagram(packet3, address, TORC_SSDP_UDP_MULTICAST_PORT);
    if (sent != (packet1.size() + packet2.size() + packet3.size()))
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Error sending 3 %1 SSDP NOTIFYs (%2)").arg(IsIPv6 ? IPv6 : IPv4, m_ipv4MulticastSocket->errorString()));
    else
        LOG(VB_NETWORK, LOG_INFO, QStringLiteral("Sent 3 %1 SSDP NOTIFYs ").arg(IsIPv6 ? IPv6 : IPv4));
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
                if (m_announceOptions.ipv6)
                    SendAnnounce(true, true);  //IPv6 alive
                m_firstAnnounceTimer = startTimer(10 * 60 * 1000);
            }
            else if (event->timerId() == m_secondAnnounceTimer)
            {
                killTimer(m_secondAnnounceTimer);
                SendAnnounce(false, true);
                if (m_announceOptions.ipv6)
                    SendAnnounce(true, true);
                m_secondAnnounceTimer = startTimer(10 * 60 * 1000);
            }
            // refresh the cache
            else if (event->timerId() == m_refreshTimer)
            {
                Refresh();
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
        LOG(VB_NETWORK, LOG_DEBUG, QStringLiteral("Raw datagram:\r\n%1").arg(data));

        // split data into lines
        QStringList lines = data.split(QStringLiteral("\r\n"), QString::SkipEmptyParts);
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

        if (type.startsWith(QStringLiteral("HTTP"), Qt::CaseInsensitive))
        {
            // response
            if (headers.contains(QStringLiteral("cache-control")))
            {
                qint64 expires = GetExpiryTime(headers.value(QStringLiteral("cache-control")));
                if (expires > 0)
                    ProcessDevice(headers, expires, true/*add*/);
            }
        }
        else if (type.startsWith(QStringLiteral("NOTIFY"), Qt::CaseInsensitive))
        {
            // notfification ssdp:alive or ssdp:bye
            if (headers.contains(QStringLiteral("nts")))
            {
                bool add = headers.value(QStringLiteral("nts")) != QStringLiteral("ssdp:byebye");

                if (add)
                {
                    if (headers.contains(QStringLiteral("cache-control")))
                    {
                        qint64 expires = GetExpiryTime(headers.value(QStringLiteral("cache-control")));
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
        else if (type.startsWith(QStringLiteral("M-SEARCH"), Qt::CaseInsensitive))
        {
            // check MAN for ssdp:discover
            if (headers.value(QStringLiteral("man")).contains(QStringLiteral("ssdp:discover"), Qt::CaseInsensitive))
            {
                // already have host address from datagram
                // need MX value in seconds
                // lazy - should be present and in the range 1-120 - but limit to 5
                int mx = qBound(1, headers.value(QStringLiteral("mx")).toInt(), 5);

                // check the target
                TorcSSDPSearchResponse::ResponseTypes types(TorcSSDPSearchResponse::None);
                QString st = headers.value(QStringLiteral("st"));
                if (st == QStringLiteral("ssdp:all"))
                {
                    types |= TorcSSDPSearchResponse::Root;
                    types |= TorcSSDPSearchResponse::UUID;
                    types |= TorcSSDPSearchResponse::Device;
                }
                else if (st == QStringLiteral("upnp:rootdevice"))
                {
                    types |= TorcSSDPSearchResponse::Root;
                }
                else if (st.startsWith(QStringLiteral("uuid:")))
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
                LOG(VB_NETWORK, LOG_INFO, QStringLiteral("Unknown MAN value in search request"));
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
    if (!m_announceOptions.port)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot send response to %1 - no port set, not advertising!")
            .arg(Response.m_responseAddress.toString()));
        return;
    }

    /* As per notify (NT -> ST)
     * Root   - ST: upnp:rootdevice                             USN: uuid:device-UUID::upnp:rootdevice
     * UUID   - ST: uuid:device-UUID                            USN: uuid:device-UUID
     * Device - ST: urn:schemas-torcapp-org:device:TorcClient:1 USN: uuid:device-UUID::urn:schemas-torcapp-org:device:TorcClient:1
    */

    // TODO this duplicates format from TorcHTTPRequest
    static const QString dateformat(QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
    static const QString reply(QStringLiteral("HTTP/1.1 200 OK\r\n"
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
                               "PRIORITY: %11\r\n%12\r\n"));

    QString date       = QDateTime::currentDateTime().toString(dateformat);
    QString uuid       = QStringLiteral("uuid:") + gLocalContext->GetUuid();
    bool ipv6          = Response.m_responseAddress.protocol() == QAbstractSocket::IPv6Protocol;
    if ((ipv6 && m_ipv6Address.isEmpty()) || (!ipv6 && m_ipv4Address.isEmpty()))
        return;
    QUdpSocket *socket = ipv6 ? m_ipv6UnicastSocket : m_ipv4UnicastSocket;
    QString raddress   = QStringLiteral("%1:%2").arg(Response.m_responseAddress.toString(), Response.m_port);
    QString secure     = m_announceOptions.secure ? QStringLiteral("s") : QStringLiteral("");
    QString secure2    = m_announceOptions.secure ? QStringLiteral("SECURE: yes\r\n") : QStringLiteral("");
    QString name       = TorcHTTPServer::ServerDescription();
    QString apiversion = TorcHTTPServices::GetVersion();
    QString starttime  = QString::number(gLocalContext->GetStartTime());
    QString priority   = QString::number(gLocalContext->GetPriority());
    int sent;
    QByteArray packet;
    if (Response.m_responseTypes.testFlag(TorcSSDPSearchResponse::Root))
    {
        packet = QString(reply).arg(date, secure, ipv6 ? m_ipv6Address : m_ipv4Address).arg(m_announceOptions.port)
                               .arg(m_serverString, QStringLiteral("upnp:rootdevice"), QStringLiteral("%1::upnp:rootdevice").arg(uuid), name, apiversion, starttime, priority, secure2).toLocal8Bit();
        sent = socket->writeDatagram(packet, Response.m_responseAddress, Response.m_port);
        if (sent != packet.size())
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Error sending SSDP root search response to %1 (%2)").arg(raddress, socket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, QStringLiteral("Sent SSDP root search response to %1").arg(raddress));
    }

    if (Response.m_responseTypes.testFlag(TorcSSDPSearchResponse::UUID))
    {
        packet = QString(reply).arg(date, secure, ipv6 ? m_ipv6Address : m_ipv4Address).arg(m_announceOptions.port)
                               .arg(m_serverString, uuid, uuid, name, apiversion, starttime, priority, secure2).toLocal8Bit();
        sent = socket->writeDatagram(packet, Response.m_responseAddress, Response.m_port);
        if (sent != packet.size())
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Error sending SSDP UUID search response to %1 (%2)").arg(raddress, socket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, QStringLiteral("Sent SSDP UUID search response to %1").arg(raddress));
    }

    if (Response.m_responseTypes.testFlag(TorcSSDPSearchResponse::Device))
    {
        packet = QString(reply).arg(date, secure, ipv6 ? m_ipv6Address : m_ipv4Address).arg(m_announceOptions.port)
                               .arg(m_serverString, TORC_ROOT_UPNP_DEVICE, uuid + "::" + TORC_ROOT_UPNP_DEVICE, name, apiversion, starttime, priority, secure2).toLocal8Bit();
        sent = socket->writeDatagram(packet, Response.m_responseAddress, Response.m_port);
        if (sent != packet.size())
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Error sending SSDP Device search response to %1 (%2)").arg(raddress, socket->errorString()));
        else
            LOG(VB_NETWORK, LOG_INFO, QStringLiteral("Sent SSDP UUID Device response to %1").arg(raddress));
    }
}

void TorcSSDP::Refresh(void)
{
    if (!m_started)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // remove stale discovered devices (if still present, they should have notified
    // a refresh)
    QVector<TorcUPNPDescription> removed;
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
    QVector<TorcUPNPDescription>::const_iterator it2 = removed.constBegin();
    for ( ; it2 != removed.constEnd(); ++it2)
    {
        QVariantMap data;
        data.insert(QStringLiteral("usn"), (*it2).GetUSN());
        TorcEvent event(Torc::ServiceWentAway, data);
        gLocalContext->Notify(event);
    }


    LOG(VB_NETWORK, LOG_INFO, QStringLiteral("Removed %1 stale cache entries").arg(removed.size()));
}

void TorcSSDP::ProcessDevice(const QMap<QString,QString> &Headers, qint64 Expires, bool Add)
{
    if (!Headers.contains(QStringLiteral("usn")) || !Headers.contains(QStringLiteral("location")) || Expires < 1)
        return;

    QString USN = Headers.value(QStringLiteral("usn"));
    QString location = Headers.value(QStringLiteral("location"));

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
        data.insert(QStringLiteral("usn"), USN);
        data.insert(QStringLiteral("address"), location);
        data.insert(QStringLiteral("name"), Headers.value(QStringLiteral("name")));
        data.insert(QStringLiteral("apiversion"), Headers.value(QStringLiteral("apiversion")));
        data.insert(QStringLiteral("starttime"), Headers.value(QStringLiteral("starttime")));
        data.insert(QStringLiteral("priority"), Headers.value(QStringLiteral("priority")));
        if (Headers.contains(QStringLiteral("secure")))
            data.insert(QStringLiteral("secure"), QStringLiteral("yes"));
        TorcEvent event(Add ? Torc::ServiceDiscovered : Torc::ServiceWentAway, data);
        gLocalContext->Notify(event);
    }
}

void TorcSSDP::StartSearch(void)
{
    gSSDPLock->lock();
    TorcHTTPServer::Status newoptions = gSearchOptions;
    gSSDPLock->unlock();

    if (m_searching && newoptions == m_searchOptions)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSDP search options unchanged - not restarting"));
        return;
    }

    StopSearch();

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Starting SSDP search for %1 devices").arg(TORC_ROOT_UPNP_DEVICE));
    m_searchOptions = newoptions;

    // schedule first search almost immediately - this is evented to ensure subsequent searches are scheduled
    m_firstSearchTimer = startTimer(1, Qt::CoarseTimer);
    // and schedule another search for after the MX time in our first request (3)
    m_secondSearchTimer = startTimer(5000, Qt::CoarseTimer);

    m_searchDebugged = false;
    m_searching = true;
}

void TorcSSDP::StopSearch(void)
{
    if (m_searching)
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Stopping SSDP search for %1 devices").arg(TORC_ROOT_UPNP_DEVICE));

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
    int index = Expires.indexOf(QStringLiteral("max-age"), 0, Qt::CaseInsensitive);
    if (index > -1)
    {
        int index2 = Expires.indexOf('=', index);
        if (index2 > -1)
        {
            int seconds = Expires.midRef(index2 + 1).toInt();
            return QDateTime::currentMSecsSinceEpoch() + 1000 * seconds;
        }
    }

    return -1;
}

TorcSSDPThread::TorcSSDPThread() : TorcQThread(QStringLiteral("SSDP"))
{
}

void TorcSSDPThread::Start(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSDP thread starting"));
    TorcSSDP::Create();
}

void TorcSSDPThread::Finish(void)
{
    TorcSSDP::Create(true/*destroy*/);
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("SSDP thread stopping"));
}
