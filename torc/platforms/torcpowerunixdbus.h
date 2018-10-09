#ifndef TORCPOWERUNIXDBUS_H
#define TORCPOWERUNIXDBUS_H

// Qt
#include <QtDBus>

// Torc
#include "torcpower.h"

class TorcPowerUnixDBus Q_DECL_FINAL : public TorcPower
{
    Q_OBJECT

  public:
    static bool Available (void);

  public:
    TorcPowerUnixDBus();
    virtual ~TorcPowerUnixDBus();

    bool DoShutdown         (void) Q_DECL_OVERRIDE;
    bool DoSuspend          (void) Q_DECL_OVERRIDE;
    bool DoHibernate        (void) Q_DECL_OVERRIDE;
    bool DoRestart          (void) Q_DECL_OVERRIDE;

  public slots:
    void DeviceAdded        (QDBusObjectPath Device);
    void DeviceRemoved      (QDBusObjectPath Device);
    void DeviceChanged      (QDBusObjectPath Device);
    void DBusError          (QDBusError      Error);
    void DBusCallback       (void);
    void Changed            (void);

  private:
    void UpdateBattery      (void);
    int  GetBatteryLevel    (const QString &Path);
    void UpdateProperties   (void);

  private:
    bool                    m_onBattery;
    QMap<QString,int>       m_devices;
    QMutex                  m_deviceLock;
    QDBusInterface          m_upowerInterface;
    QDBusInterface          m_consoleInterface;
};

#endif // TORCPOWERUNIXDBUS_H
