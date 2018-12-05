#ifndef TORCBONJOUR_H
#define TORCBONJOUR_H

// Qt
#include <QMutex>
#include <QObject>
#include <QHostInfo>
#include <QSocketNotifier>

// DNS Service Discovery
#include <dns_sd.h>

#define TORC_BONJOUR_HOST      QStringLiteral("host")
#define TORC_BONJOUR_ADDRESSES QStringLiteral("addresses")
#define TORC_BONJOUR_TXT       QStringLiteral("txtrecords")
#define TORC_BONJOUR_NAME      QStringLiteral("name")
#define TORC_BONJOUR_TYPE      QStringLiteral("type")
#define TORC_BONJOUR_PORT      QStringLiteral("port")

class TorcBonjour;

class TorcBonjourService
{
  public:
    enum ServiceType
    {
        Service, //!< A service being advertised by this application
        Browse,  //!< An external service which we are actively trying to discover
        Resolve  //!< Address resolution for a discovered service
    };

  public:
    TorcBonjourService();
    TorcBonjourService(const TorcBonjourService &Other);
    TorcBonjourService(ServiceType BonjourType, DNSServiceRef DNSSRef, const QByteArray &Name, const QByteArray &Type);
    TorcBonjourService(ServiceType BonjourType, const QByteArray &Name, const QByteArray &Type, const QByteArray &Domain, uint32_t InterfaceIndex);
    TorcBonjourService& operator =(const TorcBonjourService &Other);
   ~TorcBonjourService() = default;
    void SetFileDescriptor(int FileDescriptor, TorcBonjour *Object);
    bool IsResolved(void);
    void Deregister(void);

  public:
    ServiceType      m_serviceType;
    DNSServiceRef    m_dnssRef;
    QByteArray       m_name;
    QByteArray       m_type;
    QByteArray       m_txt;
    QByteArray       m_domain;
    uint32_t         m_interfaceIndex;
    QByteArray       m_host;
    QList<QHostAddress> m_ipAddresses;
    int              m_port;
    int              m_lookupID;
    int              m_fd;
    QSocketNotifier *m_socketNotifier;
};

class TorcBonjour : public QObject
{
    Q_OBJECT

  public:
    static  TorcBonjour*     Instance (void);
    static  void             Suspend  (bool Suspend);
    static  void             TearDown (void);
    static  QByteArray       MapToTxtRecord (const QMap<QByteArray,QByteArray> &Map);
    static  QMap<QByteArray,QByteArray> TxtRecordToMap(const QByteArray &TxtRecord);

    quint32 Register        (quint16 Port, const QByteArray &Type,
                             const QByteArray &Name, const QMap<QByteArray,QByteArray> &TxtRecords, quint32 Reference = 0);
    quint32 Register        (quint16 Port, const QByteArray &Type,
                             const QByteArray &Name, const QByteArray &Txt, quint32 Reference = 0);
    quint32 Browse          (const QByteArray &Type, quint32 Reference = 0);
    void    Deregister      (quint32 Reference);

    // callback handlers
    void AddBrowseResult(DNSServiceRef Reference, const TorcBonjourService &Service);
    void RemoveBrowseResult(DNSServiceRef Reference, const TorcBonjourService &Service);
    void Resolve(DNSServiceRef Reference, DNSServiceErrorType ErrorType, const char *Fullname,
                 const char *HostTarget, uint16_t Port, uint16_t TxtLen,
                 const unsigned char *TxtRecord);

  public slots:
    void    socketReadyRead (int Socket);

  private slots:
    void    SuspendPriv     (bool Suspend);
    void    HostLookup      (const QHostInfo &HostInfo);


  protected:
    TorcBonjour();
   ~TorcBonjour();

  private:
    bool                             m_suspended;
    QMutex                           m_serviceLock;
    QMap<quint32,TorcBonjourService> m_services;
    QMutex                           m_discoveredLock;
    QMap<quint32,TorcBonjourService> m_discoveredServices;
};

#endif
