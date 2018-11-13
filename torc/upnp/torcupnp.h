#ifndef TORCUPNP_H
#define TORCUPNP_H

// Torc
#include "torclocalcontext.h"

#define TORC_ROOT_UPNP_DEVICE QString("urn:schemas-torcapp-org:device:TorcClient:1")

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
