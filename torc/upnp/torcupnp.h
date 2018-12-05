#ifndef TORCUPNP_H
#define TORCUPNP_H

// Torc
#include "torclocalcontext.h"

#define TORC_ROOT_UPNP_DEVICE QStringLiteral("urn:schemas-torcapp-org:device:TorcClient:1")

#define _UUID                 "uuid"
#define _APIVERSION           "apiversion"
#define _PRIORITY             "priority"
#define _STARTTIME            "starttime"
#define _SECURE               "secure"

#define TORC_UUID             QStringLiteral(_UUID)
#define TORC_NAME             QStringLiteral("name")
#define TORC_PORT             QStringLiteral("port")
#define TORC_APIVERSION       QStringLiteral(_APIVERSION)
#define TORC_PRIORITY         QStringLiteral(_PRIORITY)
#define TORC_STARTTIME        QStringLiteral(_STARTTIME)
#define TORC_ADDRESS          QStringLiteral("address")
#define TORC_SECURE           QStringLiteral(_SECURE)
#define TORC_USN              QStringLiteral("usn")
#define TORC_YES              QStringLiteral("yes")

#define TORC_UUID_B           QByteArrayLiteral(_UUID)
#define TORC_APIVERSION_B     QByteArrayLiteral(_APIVERSION)
#define TORC_PRIORITY_B       QByteArrayLiteral(_PRIORITY)
#define TORC_STARTTIME_B      QByteArrayLiteral(_STARTTIME)
#define TORC_SECURE_B         QByteArrayLiteral(_SECURE)

class TorcUPNPDescription final
{
  public:
    TorcUPNPDescription ();
    TorcUPNPDescription (const QString &USN, const QString &Location, qint64 Expires);
    QString GetUSN      (void) const;
    QString GetLocation (void) const;
    qint64  GetExpiry   (void) const;
    void    SetExpiry   (qint64 Expires);

  private:
    QString m_usn;
    QString m_location;
    qint64  m_expiry;
};

class TorcUPNP final
{
  public:
    static  QString     UUIDFromUSN (const QString &USN);
};

Q_DECLARE_METATYPE(TorcUPNPDescription)

#endif // TORCUPNP_H
