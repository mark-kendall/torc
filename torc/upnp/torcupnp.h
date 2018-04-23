#ifndef TORCUPNP_H
#define TORCUPNP_H

// Torc
#include "torclocalcontext.h"

#define TORC_ROOT_UPNP_DEVICE QString("urn:schemas-torcapp-org:device:TorcClient:1")

class TorcUPNPDescription
{
  public:
    TorcUPNPDescription ();
    TorcUPNPDescription (const QString &USN, const QString &Type, const QString &Location, qint64 Expires);
    QString GetUSN      (void) const;
    QString GetType     (void) const;
    QString GetLocation (void) const;
    qint64  GetExpiry   (void) const;
    void    SetExpiry   (qint64 Expires);

    bool operator == (const TorcUPNPDescription &Other) const;

  private:
    QString m_usn;
    QString m_type;
    QString m_location;
    qint64  m_expiry;
};

class TorcUPNP
{
  public:
    static  QString     UUIDFromUSN (const QString &USN);
};

Q_DECLARE_METATYPE(TorcUPNPDescription);

#endif // TORCUPNP_H
