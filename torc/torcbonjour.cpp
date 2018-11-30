/* Class TorcBonjour
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QCoreApplication>
#include <QtEndian>
#include <QMap>

// Torc
#include "torclogging.h"
#include "torcevent.h"
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torcnetwork.h"
#include "torcbonjour.h"

// Std
#include <stdlib.h>
#include <arpa/inet.h>

TorcBonjour* gBonjour = nullptr;                            //!< Global TorcBonjour singleton
QMutex*      gBonjourLock = new QMutex(QMutex::Recursive);  //!< Lock around access to gBonjour

void DNSSD_API BonjourRegisterCallback (DNSServiceRef        Ref,
                                        DNSServiceFlags      Flags,
                                        DNSServiceErrorType  Errorcode,
                                        const char          *Name,
                                        const char          *Type,
                                        const char          *Domain,
                                        void                *Object);
void DNSSD_API BonjourBrowseCallback   (DNSServiceRef        Ref,
                                        DNSServiceFlags      Flags,
                                        uint32_t             InterfaceIndex,
                                        DNSServiceErrorType  ErrorCode,
                                        const char          *Name,
                                        const char          *Type,
                                        const char          *Domain,
                                        void                *Object);
void DNSSD_API BonjourResolveCallback  (DNSServiceRef        Ref,
                                        DNSServiceFlags      Flags,
                                        uint32_t             InterfaceIndex,
                                        DNSServiceErrorType  ErrorCode,
                                        const char          *Fullname,
                                        const char          *HostTarget,
                                        uint16_t             Port,
                                        uint16_t             TxtLen,
                                        const unsigned char *TxtRecord,
                                        void                *Object);

/*! \class TorcBonjourService
 *  \brief Wrapper around a DNS service reference, either advertised or discovered.
 *
 * TorcBonjourService takes ownership of both the DNSServiceRef and QSocketNotifier object - to
 * ensure resources are properly released, Deregister must be called.
 *
 * \sa TorcBonjour
*/
TorcBonjourService::TorcBonjourService()
  : m_serviceType(Service),
    m_dnssRef(nullptr),
    m_name(QByteArray()),
    m_type(QByteArray()),
    m_txt(QByteArray()),
    m_domain(QByteArray()),
    m_interfaceIndex(0),
    m_host(QByteArray()),
    m_ipAddresses(QList<QHostAddress>()),
    m_port(0),
    m_lookupID(-1),
    m_fd(-1),
    m_socketNotifier(nullptr)
{
    // the default constructor only exists to keep QMap happy - it should never be used
    LOG(VB_GENERAL, LOG_WARNING, "Invalid TorcBonjourService object");
}

TorcBonjourService::TorcBonjourService(const TorcBonjourService &Other)
  : m_serviceType(Other.m_serviceType),
    m_dnssRef(Other.m_dnssRef),
    m_name(Other.m_name),
    m_type(Other.m_type),
    m_txt(Other.m_txt),
    m_domain(Other.m_domain),
    m_interfaceIndex(Other.m_interfaceIndex),
    m_host(Other.m_host),
    m_ipAddresses(Other.m_ipAddresses),
    m_port(0),             // NB
    m_lookupID(-1),        // NB
    m_fd(-1),              // NB
    m_socketNotifier(nullptr) // NB
{
}

TorcBonjourService& TorcBonjourService::operator =(const TorcBonjourService &Other)
{
    m_serviceType    = Other.m_serviceType;
    m_dnssRef        = Other.m_dnssRef;
    m_name           = Other.m_name;
    m_type           = Other.m_type;
    m_txt            = Other.m_txt;
    m_domain         = Other.m_domain;
    m_interfaceIndex = Other.m_interfaceIndex;
    m_host           = Other.m_host;
    m_ipAddresses    = Other.m_ipAddresses;
    m_port           = 0;    // NB
    m_lookupID       = -1;   // NB
    m_fd             = -1;   // NB
    m_socketNotifier = nullptr; // NB
    return *this;
}

TorcBonjourService::TorcBonjourService(ServiceType BonjourType, DNSServiceRef DNSSRef, const QByteArray &Name, const QByteArray &Type)
  : m_serviceType(BonjourType),
    m_dnssRef(DNSSRef),
    m_name(Name),
    m_type(Type),
    m_txt(QByteArray()),
    m_domain(QByteArray()),
    m_interfaceIndex(0),
    m_host(QByteArray()),
    m_ipAddresses(QList<QHostAddress>()),
    m_port(0),
    m_lookupID(-1),
    m_fd(-1),
    m_socketNotifier(nullptr)
{
}

TorcBonjourService::TorcBonjourService(ServiceType BonjourType,
                   const QByteArray &Name, const QByteArray &Type,
                   const QByteArray &Domain, uint32_t InterfaceIndex)
  : m_serviceType(BonjourType),
    m_dnssRef(nullptr),
    m_name(Name),
    m_type(Type),
    m_txt(QByteArray()),
    m_domain(Domain),
    m_interfaceIndex(InterfaceIndex),
    m_host(QByteArray()),
    m_ipAddresses(QList<QHostAddress>()),
    m_port(0),
    m_lookupID(-1),
    m_fd(-1),
    m_socketNotifier(nullptr)
{
}

/*! \fn    TorcBonjourService::SetFileDescriptor
 *  \brief Sets the file descriptor and creates a QSocketNotifier to listen for socket events
*/
void TorcBonjourService::SetFileDescriptor(int FileDescriptor, QObject *Object)
{
    if (FileDescriptor != -1 && Object)
    {
        m_fd = FileDescriptor;
        m_socketNotifier = new QSocketNotifier(FileDescriptor, QSocketNotifier::Read, Object);
        QObject::connect(m_socketNotifier, SIGNAL(activated(int)),
                         Object, SLOT(socketReadyRead(int)));
        m_socketNotifier->setEnabled(true);
    }
}

/*! \fn    TorcBonjourService::IsResolved
 *  \brief Returns true when the service has been fully resolved to an IP address and port.
*/
bool TorcBonjourService::IsResolved(void)
{
    return m_port && !m_ipAddresses.isEmpty();
}

/*! \fn    TorcBonjourService::Deregister
 *  \brief Release all resources associated with this service.
 */
void TorcBonjourService::Deregister(void)
{
    if (m_socketNotifier)
        m_socketNotifier->setEnabled(false);

    if (m_lookupID != -1)
    {
        LOG(VB_NETWORK, LOG_WARNING, QString("Host lookup for '%1' is not finished - cancelling").arg(m_host.data()));
        QHostInfo::abortHostLookup(m_lookupID);
        m_lookupID = -1;
    }

    if (m_dnssRef)
    {
        // Unregister
        if (m_serviceType == Browse)
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Cancelling browse for '%1'")
                .arg(m_type.data()));
        }
        else if (m_serviceType == Service)
        {
            LOG(VB_GENERAL, LOG_INFO,
                QString("De-registering service '%1' on '%2'")
                .arg(m_type.data()).arg(m_name.data()));
        }
        else if (m_serviceType == Resolve)
        {
            LOG(VB_NETWORK, LOG_INFO, QString("Cancelling resolve for '%1'")
                .arg(m_type.data()));
        }

        DNSServiceRefDeallocate(m_dnssRef);
        m_dnssRef = nullptr;
    }

    if (m_socketNotifier)
        m_socketNotifier->deleteLater();
    m_socketNotifier = nullptr;
}

/*! \fn    BonjourRegisterCallback
 *  \brief Handles registration callbacks from Bonjour
*/
void BonjourRegisterCallback(DNSServiceRef Ref, DNSServiceFlags Flags,
                     DNSServiceErrorType Errorcode,
                     const char *Name,   const char *Type,
                     const char *Domain, void *Object)
{
    (void)Ref;
    (void)Flags;
    (void)Domain;

    TorcBonjour *bonjour = static_cast<TorcBonjour*>(Object);
    if (kDNSServiceErr_NoError != Errorcode)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Callback Error: %1").arg(Errorcode));
    }
    else if (bonjour)
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Service registration complete: name '%1' type '%2'")
            .arg(QString::fromUtf8(Name)).arg(QString::fromUtf8(Type)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("BonjourRegisterCallback for unknown object."));
    }
}

/*! \fn    BonjourBrowseCallback
 *  \brief Handles browse callbacks from Bonjour
*/
void BonjourBrowseCallback(DNSServiceRef Ref, DNSServiceFlags Flags,
                           uint32_t InterfaceIndex,
                           DNSServiceErrorType ErrorCode, const char *Name,
                           const char *Type, const char *Domain, void *Object)
{
    TorcBonjour *bonjour = static_cast<TorcBonjour*>(Object);
    if (!bonjour)
    {
        LOG(VB_GENERAL, LOG_ERR, "Received browse callback for unknown object");
        return;
    }

    if (ErrorCode != kDNSServiceErr_NoError)
    {
        LOG(VB_GENERAL, LOG_ERR, "Browse callback error");
        return;
    }

    TorcBonjourService service(TorcBonjourService::Resolve, Name, Type, Domain, InterfaceIndex);
    if (Flags & kDNSServiceFlagsAdd)
        bonjour->AddBrowseResult(Ref, service);
    else
        bonjour->RemoveBrowseResult(Ref, service);

    if (!(Flags & kDNSServiceFlagsMoreComing))
    {
        // no idea
    }
}

/*! \fn    BonjourResolveCallback
 *  \brief Handles address resolution callbacks from Bonjour
*/
void BonjourResolveCallback(DNSServiceRef Ref, DNSServiceFlags Flags,
                            uint32_t InterfaceIndex, DNSServiceErrorType ErrorCode,
                            const char *Fullname, const char *HostTarget,
                            uint16_t Port, uint16_t TxtLen,
                            const unsigned char *TxtRecord, void *Object)
{
    (void)Flags;
    (void) InterfaceIndex;

    TorcBonjour *bonjour = static_cast<TorcBonjour*>(Object);
    if (!bonjour)
    {
        LOG(VB_GENERAL, LOG_ERR, "Received resolve callback for unknown object");
        return;
    }

    bonjour->Resolve(Ref, ErrorCode, Fullname, HostTarget, Port, TxtLen, TxtRecord);
}

/*! \class TorcBonjour
 *  \brief Advertises and searches for services via Bonjour (aka Zero Configuration Networking/Avahi)
 *
 * TorcBonjour returns opaque references for use by client objects. These are guaranteed to remain
 * unchanged across network outages and other suspension events. Hence clients do not need to explicitly
 * monitor network status and cancel advertisements and/or browse requests.
 *
 * A ServiceDiscovered event is sent (via TorcLocalContext) when a device is discovered and a ServiceWentAway
 * event when it is no longer available. Clients must listen for such events via TorcLocalContext::AddObserver(this).
 *
 * \sa gBonjour
 * \sa gBonjourLock
 * \sa TorcBonjourService
*/

/*! \fn    TorcBonjour::Instance
 *  \brief Returns the global TorcBonjour singleton.
 *
 * If it does not exist, it will be created.
*/
TorcBonjour* TorcBonjour::Instance(void)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        return gBonjour;

    gBonjour = new TorcBonjour();
    return gBonjour;
}

/*! \fn    TorcBonjour::Suspend
 *  \brief Suspends all Bonjour activities.
*/
void TorcBonjour::Suspend(bool Suspend)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        QMetaObject::invokeMethod(gBonjour, "SuspendPriv", Qt::AutoConnection, Q_ARG(bool, Suspend));
}

/*! \fn    TorcBonjour::TearDown
 *  \brief Destroys the global TorcBonjour singleton.
*/
void TorcBonjour::TearDown(void)
{
    QMutexLocker locker(gBonjourLock);
    delete gBonjour;
    gBonjour = nullptr;
}

/*! \fn    TorcBonjour::MapToTxtRecord
 *  \brief Serialises a QMap into a Bonjour Txt record format.
*/
QByteArray TorcBonjour::MapToTxtRecord(const QMap<QByteArray, QByteArray> &Map)
{
    QByteArray result;

    QMap<QByteArray,QByteArray>::const_iterator it = Map.begin();
    for ( ; it != Map.end(); ++it)
    {
        QByteArray record(1, it.key().size() + it.value().size() + 1);
        record.append(it.key() + "=" + it.value());
        result.append(record);
    }

    return result;
}

/*! \fn    TorcBonjour::TxtRecordToMap
 *  \brief Extracts a QMap from a properly formatted Bonjour Txt record
*/
QMap<QByteArray,QByteArray> TorcBonjour::TxtRecordToMap(const QByteArray &TxtRecord)
{
    QMap<QByteArray,QByteArray> result;

    int position = 0;
    while (position < TxtRecord.size())
    {
        int size = TxtRecord[position++];
        QList<QByteArray> records = TxtRecord.mid(position, size).split('=');
        position += size;

        if (records.size() == 2)
            result.insert(records[0], records[1]);
    }

    return result;
}

TorcBonjour::TorcBonjour()
  : QObject(nullptr),
    m_suspended(false),
    m_serviceLock(QMutex::Recursive),
    m_services(QMap<quint32,TorcBonjourService>()),
    m_discoveredLock(QMutex::Recursive),
    m_discoveredServices(QMap<quint32,TorcBonjourService>())
{
    // the Avahi compatability layer on *nix will spam us with warnings
    qputenv("AVAHI_COMPAT_NOWARN", "1");

    // listen for network enabled/disabled events
    gLocalContext->AddObserver(this);
}

TorcBonjour::~TorcBonjour()
{
    // stop listening for network events
    gLocalContext->RemoveObserver(this);

    LOG(VB_GENERAL, LOG_INFO, "Closing Bonjour");

    // deregister any outstanding services (should be empty)
    {
        QMutexLocker locker(&m_serviceLock);
        if (!m_suspended)
        {
            QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
            for (; it != m_services.end(); ++it)
                (*it).Deregister();
        }
        m_services.clear();
    }

    // deallocate resolve queries
    {
        QMutexLocker locker(&m_discoveredLock);
        QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
        for (; it != m_discoveredServices.end(); ++it)
            (*it).Deregister();
        m_discoveredServices.clear();
    }
}

/*! \fn    TorcBonjour::Register
 *  \brief Register a service with Bonjour.
 *
 * Register the service described by Name, Type, Txt and Port. If Bonjour
 * activities are currently suspended, the details are saved and processed
 * when Bonjour is available - in which case a reference is still returned
 * (i.e. it does not return an error).
 *
 * The service will be advertised on all known system interfaces as available on port Port.
 *
 * \param Port      The port number the service is available on (e.g. 80).
 * \param Type      A string describing the Bonjour service type (e.g. _http._tcp).
 * \param Name      A string containing a user friendly name for the service (e.g. webserver).
 * \param Txt       A pre-formatted QByteArray containing the Txt records for this service.
 * \param Reference Used to re-create a service registration following a Resume.
 * \returns An opaque reference to be passed to Deregister when complete or 0 on error.
 *
 * \sa Deregister
 * \sa TorcBonjour::MapToTxtRecord
*/
quint32 TorcBonjour::Register(quint16 Port, const QByteArray &Type, const QByteArray &Name,
                              const QMap<QByteArray, QByteArray> &TxtRecords, quint32 Reference /*=0*/)
{
    QByteArray txt = MapToTxtRecord(TxtRecords);
    return Register(Port, Type, Name, txt, Reference);
}

quint32 TorcBonjour::Register(quint16 Port, const QByteArray &Type, const QByteArray &Name, const QByteArray &Txt, quint32 Reference)
{
    if (m_suspended)
    {
        QMutexLocker locker(&m_serviceLock);

        TorcBonjourService service(TorcBonjourService::Service, nullptr, Name, Type);
        service.m_txt  = Txt;
        service.m_port = Port;
        quint32 reference = Reference;
        // find a unique reference
        while (!reference || m_services.contains(reference))
            reference++;
        m_services.insert(reference, service);

        LOG(VB_GENERAL, LOG_ERR, "Bonjour service registration deferred until Bonjour resumed");
        return reference;
    }

    DNSServiceRef dnssref = nullptr;
    DNSServiceErrorType result = kDNSServiceErr_NameConflict;
    int tries = 0;

    // the avahi compatability layer doesn't appear to be automatically renaming as it should
    while (tries < 20 && kDNSServiceErr_NameConflict == result)
    {
        QByteArray name(Name);
        if (tries > 0)
            name.append(QString(" [%1]").arg(tries + 1));
        result = DNSServiceRegister(&dnssref, 0,
                                    0, (const char*)name.data(),
                                    (const char*)Type.data(),
                                    nullptr, nullptr, htons(Port), Txt.size(), (void*)Txt.data(),
                                    BonjourRegisterCallback, this);
        tries++;
    }

    if (kDNSServiceErr_NoError != result)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Register error: %1").arg(result));
        if (dnssref)
            DNSServiceRefDeallocate(dnssref);
    }
    else
    {
        QMutexLocker locker(&m_serviceLock);
        quint32 reference = Reference;
        while (!reference || m_services.contains(reference))
            reference++;
        TorcBonjourService service(TorcBonjourService::Service, dnssref, Name, Type);
        service.m_txt  = Txt;
        service.m_port = Port;
        m_services.insert(reference, service);
        m_services[reference].SetFileDescriptor(DNSServiceRefSockFD(dnssref), this);
        return reference;
    }

    LOG(VB_GENERAL, LOG_ERR, "Failed to register service.");
    return 0;
}

/*! \fn TorcBonjour::Browse
 *  \brief Search for a service advertised via Bonjour
 *
 * Initiates a browse request for the service named by Type. The requestor must listen
 * for the Torc::ServiceDiscovered and Torc::ServiceWentAway events.
 *
 * \param   Type      A string describing the Bonjour service type (e.g. _http._tcp).
 * \param   Reference Used to re-create a Browse following a Resume.
 * \returns An opaque reference to be used via Deregister on success, 0 on failure.
 *
 * \sa Deregister
*/
quint32 TorcBonjour::Browse(const QByteArray &Type, quint32 Reference /*=0*/)
{
    static QByteArray dummy("browser");

    if (m_suspended)
    {
        QMutexLocker locker(&m_serviceLock);

        TorcBonjourService service(TorcBonjourService::Browse, nullptr, dummy, Type);
        quint32 reference = Reference;
        while (!reference || m_services.contains(reference))
            reference++;
        m_services.insert(reference, service);

        LOG(VB_GENERAL, LOG_ERR, "Bonjour browse request deferred until Bonjour resumed");
        return reference;
    }

    DNSServiceRef dnssref = nullptr;
    DNSServiceErrorType result = DNSServiceBrowse(&dnssref, 0,
                                                  kDNSServiceInterfaceIndexAny,
                                                  Type, nullptr, BonjourBrowseCallback, this);
    if (kDNSServiceErr_NoError != result)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Browse error: %1").arg(result));
        if (dnssref)
            DNSServiceRefDeallocate(dnssref);
    }
    else
    {
        QMutexLocker locker(&m_serviceLock);
        quint32 reference = Reference;
        while (!reference || m_services.contains(reference))
            reference++;
        TorcBonjourService service(TorcBonjourService::Browse, dnssref, dummy, Type);
        m_services.insert(reference, service);
        m_services[reference].SetFileDescriptor(DNSServiceRefSockFD(dnssref), this);
        LOG(VB_GENERAL, LOG_INFO, QString("Browsing for '%1'").arg(Type.data()));
        return reference;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Failed to browse for '%1'").arg(Type.data()));
    return 0;
}

/*! \fn    TorcBonjour::Deregister
 *  \brief Cancel a Bonjour service registration or browse request.
 *
 * \param Reference A TorcBonjour reference previously obtained via a call to Register
 * \sa Register
 * \sa Browse
*/
void TorcBonjour::Deregister(quint32 Reference)
{
    if (!Reference)
        return;

    QByteArray type;

    {
        QMutexLocker locker(&m_serviceLock);
        if (m_services.contains(Reference))
        {
            type = m_services[Reference].m_type;
            m_services[Reference].Deregister();
            m_services.remove(Reference);
        }
    }

    if (type.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Trying to de-register an unknown service.");
        return;
    }

    // Remove any resolve requests associated with this type
    {
        QMutexLocker locker(&m_discoveredLock);
        QMutableMapIterator<quint32,TorcBonjourService> it(m_discoveredServices);
        while (it.hasNext())
        {
            it.next();
            if (it.value().m_type == type)
            {
                it.value().Deregister();
                it.remove();
            }
        }
    }
}

/*! \fn    TorcBonjour::socketReadyRead
 *  \brief Read from a socket
 *
 * TorcBonjourPriv does not inherit QObject hence needs TorcBonjour to handle
 * slots and events.
*/
void TorcBonjour::socketReadyRead(int Socket)
{
    {
        // match Socket to an announced service
        QMutexLocker lock(&m_serviceLock);
        QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
        for ( ; it != m_services.end(); ++it)
        {
            if ((*it).m_fd == Socket)
            {
                DNSServiceErrorType res = DNSServiceProcessResult((*it).m_dnssRef);
                if (kDNSServiceErr_NoError != res)
                    LOG(VB_GENERAL, LOG_ERR, QString("Read Error: %1").arg(res));
                return;
            }
        }
    }

    {
        // match Socket to a discovered service
        QMutexLocker lock(&m_discoveredLock);
        QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
        for ( ; it != m_discoveredServices.end(); ++it)
        {
            if ((*it).m_fd == Socket)
            {
                DNSServiceErrorType res = DNSServiceProcessResult((*it).m_dnssRef);
                if (kDNSServiceErr_NoError != res)
                    LOG(VB_GENERAL, LOG_ERR, QString("Read Error: %1").arg(res));
                return;
            }
        }
    }

    LOG(VB_GENERAL, LOG_WARNING, "Read request on unknown socket");
}

/*! \fn    TorcBonjour::SuspendPriv
 *  \brief Suspend all Bonjour activities.
 *
 * This method will deregister all existing service announcements but retain
 * the details for each. On resumption, services are then re-registered.
 *
 * \sa Resume
*/

void TorcBonjour::SuspendPriv(bool Suspend)
{
    if (Suspend)
    {
        if (m_suspended)
        {
            LOG(VB_GENERAL, LOG_INFO, "Bonjour already suspended");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "Suspending bonjour activities");

        m_suspended = true;

        {
            QMutexLocker locker(&m_serviceLock);

            // close the services but retain the necessary details
            QMap<quint32,TorcBonjourService> services;
            QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
            for (; it != m_services.end(); ++it)
            {
                TorcBonjourService service((*it).m_serviceType, nullptr, (*it).m_name, (*it).m_type);
                service.m_txt  = (*it).m_txt;
                service.m_port = (*it).m_port;
                (*it).Deregister();
                services.insert(it.key(), service);
            }

            m_services = services;
        }
    }
    else
    {
        if (!m_suspended)
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot resume - not suspended");
            return;
        }

        LOG(VB_GENERAL, LOG_INFO, "Resuming bonjour activities");

        {
            QMutexLocker locker(&m_serviceLock);

            m_suspended = false;

            QMap<quint32,TorcBonjourService> services = m_services;
            m_services.clear();

            QMap<quint32,TorcBonjourService>::iterator it = services.begin();
            for (; it != services.end(); ++it)
            {
                if ((*it).m_serviceType == TorcBonjourService::Service)
                    (void)Register((*it).m_port, (*it).m_type, (*it).m_name, (*it).m_txt, it.key());
                else
                    (void)Browse((*it).m_type, it.key());
            }
        }
    }
}

/*! \fn    TorcBonjour::AddBrowseResult
 *  \brief Handle newly discovered service.
 *
 * Services that have not been seen before are added to the list of known services
 * and a name resolution is kicked off.
*/
void TorcBonjour::AddBrowseResult(DNSServiceRef Reference, const TorcBonjourService &Service)
{
    {
        // validate against known browsers
        QMutexLocker locker(&m_serviceLock);
        bool found = false;
        QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
        for ( ; it != m_services.end(); ++it)
        {
            if ((*it).m_dnssRef == Reference)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG(VB_GENERAL, LOG_INFO, "Browser result for unknown browser");
            return;
        }
    }

    {
        // have we already seen this service?
        QMutexLocker locker(&m_discoveredLock);
        QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
        for( ; it != m_discoveredServices.end(); ++it)
        {
            if ((*it).m_name == Service.m_name &&
                (*it).m_type == Service.m_type &&
                (*it).m_domain == Service.m_domain &&
                (*it).m_interfaceIndex == Service.m_interfaceIndex)
            {
                LOG(VB_NETWORK, LOG_INFO, QString("Service '%1' already discovered - ignoring")
                    .arg(Service.m_name.data()));
                return;
            }
        }

        // kick off resolve
        DNSServiceRef reference = nullptr;
        DNSServiceErrorType error =
            DNSServiceResolve(&reference, 0, Service.m_interfaceIndex,
                              Service.m_name.data(), Service.m_type.data(),
                              Service.m_domain.data(), BonjourResolveCallback,
                              this);
        if (error != kDNSServiceErr_NoError)
        {
            LOG(VB_GENERAL, LOG_ERR, "Service resolution call failed");
            if (reference)
                DNSServiceRefDeallocate(reference);
        }
        else
        {
            // add it to our list
            TorcBonjourService service = Service;
            quint32 ref = 1;
            while (m_discoveredServices.contains(ref))
                ref++;
            service.m_dnssRef = reference;
            m_discoveredServices.insert(ref, service);
            m_discoveredServices[ref].SetFileDescriptor(DNSServiceRefSockFD(reference), this);
            LOG(VB_NETWORK, LOG_INFO, QString("Resolving '%1'").arg(service.m_name.data()));
        }
    }
}

/*! \fn    TorcBonjour::RemoveBrowseResult
 *  \brief Handle removed services.
*/
void TorcBonjour::RemoveBrowseResult(DNSServiceRef Reference,
                        const TorcBonjourService &Service)
{
    {
        // validate against known browsers
        QMutexLocker locker(&m_serviceLock);
        bool found = false;
        QMap<quint32,TorcBonjourService>::iterator it = m_services.begin();
        for ( ; it != m_services.end(); ++it)
        {
            if ((*it).m_dnssRef == Reference)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG(VB_GENERAL, LOG_INFO, "Browser result for unknown browser");
            return;
        }
    }

    {
        // validate against known services
        QMutexLocker locker(&m_discoveredLock);
        QMutableMapIterator<quint32,TorcBonjourService> it(m_discoveredServices);
        while (it.hasNext())
        {
            it.next();
            if (it.value().m_type == Service.m_type &&
                it.value().m_name == Service.m_name &&
                it.value().m_domain == Service.m_domain &&
                it.value().m_interfaceIndex == Service.m_interfaceIndex)
            {
                QVariantMap data;
                data.insert("name", it.value().m_name.data());
                data.insert("type", it.value().m_type.data());
                data.insert("txtrecords", it.value().m_txt);
                data.insert("host", it.value().m_host);
                TorcEvent event(Torc::ServiceWentAway, data);
                gLocalContext->Notify(event);

                LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' on '%2' went away")
                    .arg(it.value().m_type.data())
                    .arg(it.value().m_host.isEmpty() ? it.value().m_name.data() : it.value().m_host.data()));
                it.value().Deregister();
                it.remove();
            }
        }
    }
}

/*! \fn    TorcBonjour::Resolve
 *  \brief Handle name resolution respones from Bonjour
 *
 * This function is called from the Bonjour library when a discovered service is resolved
 * to a target and port. Before we can use this target, we attempt to discover its IP addresses
 * via QHostInfo.
 *
 * \sa HostLookup
*/
void TorcBonjour::Resolve(DNSServiceRef Reference, DNSServiceErrorType ErrorType, const char *Fullname,
             const char *HostTarget, uint16_t Port, uint16_t TxtLen,
             const unsigned char *TxtRecord)
{
    (void)Fullname;

    if (!Reference)
        return;

    {
        QMutexLocker locker(&m_discoveredLock);
        QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
        for( ; it != m_discoveredServices.end(); ++it)
        {
            if ((*it).m_dnssRef == Reference)
            {
                if (ErrorType != kDNSServiceErr_NoError)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Failed to resolve '%1' (Error %2)").arg((*it).m_name.data()).arg(ErrorType));
                    QVariantMap data;
                    data.insert("name", (*it).m_name.data());
                    data.insert("type", (*it).m_type.data());
                    data.insert("port", (*it).m_port);
                    data.insert("txtrecords", (*it).m_txt);
                    data.insert("host", (*it).m_host.data());
                    // as per HostLookup below - resolve may have failed due to a network disruption and the service is no
                    // longer available. Signal removal - if it wasn't already known it won't matter.
                    TorcEvent event(Torc::ServiceWentAway, data);
                    gLocalContext->Notify(event);
                    return;
                }

                uint16_t port = ntohs(Port);
                (*it).m_host = HostTarget;
                (*it).m_port = port;
                LOG(VB_NETWORK, LOG_INFO, QString("%1 (%2) resolved to %3:%4")
                    .arg((*it).m_name.data()).arg((*it).m_type.data()).arg(HostTarget).arg(port));
                QString name(HostTarget);
                (*it).m_lookupID = QHostInfo::lookupHost(name, this, SLOT(HostLookup(QHostInfo)));
                (*it).m_txt      = QByteArray((const char *)TxtRecord, TxtLen);
            }
        }
    }
}

/*! \fn    TorcBonjour::HostLookup
 *  \brief Handle host lookup responses from Qt.
 *
 * If the service is fully resolved to one or more IP addresses, notify the new
 * service with a Torc::ServiceDiscovered event.
*/
void TorcBonjour::HostLookup(const QHostInfo &HostInfo)
{
    // search for the lookup id
    {
        QMutexLocker locker(&m_discoveredLock);
        QMap<quint32,TorcBonjourService>::iterator it = m_discoveredServices.begin();
        for ( ; it != m_discoveredServices.end(); ++it)
        {
            if ((*it).m_lookupID == HostInfo.lookupId())
            {
                (*it).m_lookupID = -1;

                // igore if errored
                if (HostInfo.error() != QHostInfo::NoError)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Lookup failed for '%1' with error '%2'").arg(HostInfo.hostName()).arg(HostInfo.errorString()));
                    return;
                }

                (*it).m_ipAddresses = HostInfo.addresses();
                LOG(VB_GENERAL, LOG_INFO, QString("Service '%1' on '%2:%3' resolved to %4 address(es) on interface %5")
                    .arg((*it).m_type.data())
                    .arg((*it).m_host.data())
                    .arg((*it).m_port)
                    .arg((*it).m_ipAddresses.size())
                    .arg((*it).m_interfaceIndex));

                QStringList addresses;

                foreach (QHostAddress address, (*it).m_ipAddresses)
                {
                    LOG(VB_NETWORK, LOG_INFO, QString("Address: %1").arg(address.toString()));
                    addresses << address.toString();
                }

                QVariantMap data;
                data.insert("name", (*it).m_name.data());
                data.insert("type", (*it).m_type.data());
                data.insert("port", (*it).m_port);
                data.insert("addresses", addresses);
                data.insert("txtrecords", (*it).m_txt);
                data.insert("host", (*it).m_host.data());
                if (!addresses.isEmpty())
                {
                    TorcEvent event(Torc::ServiceDiscovered, data);
                    gLocalContext->Notify(event);
                }
                else
                {
                    LOG(VB_GENERAL, LOG_WARNING, QString("No valid addresses resolved for Service '%1' on '%2:%3'").arg((*it).m_type.data())
                        .arg((*it).m_host.data())
                        .arg((*it).m_port));
                    // NB this is experimental!!!
                    TorcEvent event(Torc::ServiceWentAway, data);
                    gLocalContext->Notify(event);
                }
            }
        }
    }
}
