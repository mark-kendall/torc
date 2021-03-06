/* Class TorcPowerUnixDBus
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

// clazy:excludeall=function-args-by-ref

// Torc
#include "torclogging.h"
#include "torcpowerunixdbus.h"

/*! \class TorcPowerUnixDBus
 *  \brief A power monitoring class for Unix based systems.
 *
 * TorcPowerUnixDBus uses the UPower and ConsoleKit DBus interfaces to monitor the system's power
 * status. UPower has replaced HAL and DeviceKit as the default power monitoring interface but additional
 * implementations for DeviceKit (and perhaps HAL) may be required for complete coverage.
 *
 * \todo Battery status monitoring is untested.
*/

bool TorcPowerUnixDBus::Available(void)
{
    static bool available = false;
    static bool checked   = false;

    if (!checked)
    {
        checked = true;

        QDBusInterface upower(QStringLiteral("org.freedesktop.UPower"),
                              QStringLiteral("/org/freedesktop/UPower"),
                              QStringLiteral("org.freedesktop.UPower"),
                              QDBusConnection::systemBus());
        QDBusInterface consolekit(QStringLiteral("org.freedesktop.ConsoleKit"),
                                  QStringLiteral("/org/freedesktop/ConsoleKit/Manager"),
                                  QStringLiteral("org.freedesktop.ConsoleKit.Manager"),
                                  QDBusConnection::systemBus());

        available = upower.isValid() && consolekit.isValid();

        if (!available)
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("UPower and ConsoleKit not available"));
    }

    return available;
}

TorcPowerUnixDBus::TorcPowerUnixDBus()
  : TorcPower(),
    m_onBattery(false),
    m_devices(QMap<QString,int>()),
    m_upowerInterface(QStringLiteral("org.freedesktop.UPower"),
                      QStringLiteral("/org/freedesktop/UPower"),
                      QStringLiteral("org.freedesktop.UPower"),
                      QDBusConnection::systemBus()),
    m_consoleInterface(QStringLiteral("org.freedesktop.ConsoleKit"),
                       QStringLiteral("/org/freedesktop/ConsoleKit/Manager"),
                       QStringLiteral("org.freedesktop.ConsoleKit.Manager"),
                       QDBusConnection::systemBus())
{
    if (m_consoleInterface.isValid())
    {
        QDBusReply<bool> shutdown = m_consoleInterface.call(QStringLiteral("CanStop"));
        if (shutdown.isValid() && shutdown.value())
            m_canShutdown->SetValue(QVariant((bool)true));

        QDBusReply<bool> restart = m_consoleInterface.call(QStringLiteral("CanRestart"));
        if (restart.isValid() && restart.value())
            m_canRestart->SetValue(QVariant((bool)true));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create ConsoleKit interface"));
    }

    // populate devices
    if (m_upowerInterface.isValid())
    {
        QDBusReply<QList<QDBusObjectPath> > devicecheck = m_upowerInterface.call(QStringLiteral("EnumerateDevices"));

        if (devicecheck.isValid())
            foreach (const QDBusObjectPath &device, devicecheck.value())
                m_devices.insert(device.path(), GetBatteryLevel(device.path()));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create UPower interface"));
    }

    // register for events
    if (!QDBusConnection::systemBus().connect(
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("Sleeping"),
         this, SLOT(Suspending())))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to register for sleep events"));
    }

    if (!QDBusConnection::systemBus().connect(
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("Resuming"),
         this, SLOT(WokeUp())))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to register for resume events"));
    }

    if (!QDBusConnection::systemBus().connect(
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("Changed"),
         this, SLOT(Changed())))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to register for Changed"));
    }

    if (!QDBusConnection::systemBus().connect(
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("DeviceChanged"), QStringLiteral("o"),
         this, SLOT(DeviceChanged(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to register for DeviceChanged"));
    }

    if (!QDBusConnection::systemBus().connect(
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("DeviceAdded"), QStringLiteral("o"),
         this, SLOT(DeviceAdded(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to register for DeviceAdded"));
    }

    if (!QDBusConnection::systemBus().connect(
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
         QStringLiteral("org.freedesktop.UPower"), QStringLiteral("DeviceRemoved"), QStringLiteral("o"),
         this, SLOT(DeviceRemoved(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to register for DeviceRemoved"));
    }

    // set battery state
    Changed();

    Debug();
}

bool TorcPowerUnixDBus::DoShutdown(void)
{
    m_httpServiceLock.lockForWrite();
    if (m_consoleInterface.isValid() && m_canShutdown->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_consoleInterface.callWithCallback(QStringLiteral("Stop"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            m_httpServiceLock.unlock();
            ShuttingDown();
            return true;
        }

        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Shutdown call failed"));
    }
    m_httpServiceLock.unlock();
    return false;
}

bool TorcPowerUnixDBus::DoSuspend(void)
{
    m_httpServiceLock.lockForWrite();
    if (m_upowerInterface.isValid() && m_canSuspend->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_upowerInterface.callWithCallback(QStringLiteral("AboutToSleep"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            if (m_upowerInterface.callWithCallback(QStringLiteral("Suspend"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            {
                m_httpServiceLock.unlock();
                Suspending();
                return true;
            }
        }

        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Suspend call failed"));
    }
    m_httpServiceLock.unlock();
    return false;
}

bool TorcPowerUnixDBus::DoHibernate(void)
{
    m_httpServiceLock.lockForWrite();
    if (m_upowerInterface.isValid() && m_canHibernate->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_upowerInterface.callWithCallback(QStringLiteral("AboutToSleep"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            if (m_upowerInterface.callWithCallback(QStringLiteral("Hibernate"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            {
                m_httpServiceLock.unlock();
                Hibernating();
                return true;
            }
        }

        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Hibernate call failed"));
    }
    m_httpServiceLock.unlock();
    return false;
}

bool TorcPowerUnixDBus::DoRestart(void)
{
    m_httpServiceLock.lockForWrite();
    if (m_consoleInterface.isValid() && m_canRestart->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_consoleInterface.callWithCallback(QStringLiteral("Restart"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            m_httpServiceLock.unlock();
            Restarting();
            return true;
        }

        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Restart call failed"));
    }
    m_httpServiceLock.unlock();
    return false;
}

void TorcPowerUnixDBus::DeviceAdded(QDBusObjectPath Device)
{
    {
        QWriteLocker locker(&m_httpServiceLock);

        if (m_devices.contains(Device.path()))
            return;

        m_devices.insert(Device.path(), GetBatteryLevel(Device.path()));
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Added UPower.Device '%1'").arg(Device.path()));
    UpdateBattery();
}

void TorcPowerUnixDBus::DeviceRemoved(QDBusObjectPath Device)
{
    {
        QWriteLocker locker(&m_httpServiceLock);

        if (!m_devices.contains(Device.path()))
            return;

        m_devices.remove(Device.path());
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Removed UPower.Device '%1'").arg(Device.path()));
    UpdateBattery();
}

void TorcPowerUnixDBus::DeviceChanged(QDBusObjectPath Device)
{
    {
        QWriteLocker locker(&m_httpServiceLock);

        if (!m_devices.contains(Device.path()))
            return;

        m_devices[Device.path()] = GetBatteryLevel(Device.path());
    }

    UpdateBattery();
}

void TorcPowerUnixDBus::DBusError(QDBusError Error)
{
    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("DBus callback error: %1, %2")
        .arg(Error.name(), Error.message().trimmed()));
}

void TorcPowerUnixDBus::DBusCallback(void)
{
}

void TorcPowerUnixDBus::Changed(void)
{
    UpdateProperties();
    UpdateBattery();
}

void TorcPowerUnixDBus::UpdateBattery(void)
{
    m_httpServiceLock.lockForWrite();
    if (m_onBattery)
    {
        qreal total = 0;
        int   count = 0;

        QMap<QString,int>::const_iterator it = m_devices.constBegin();
        for ( ; it != m_devices.constEnd(); ++it)
        {
            if (it.value() >= 0 && it.value() <= 100)
            {
                count++;
                total += (qreal)it.value();
            }
        }

        if (count > 0)
        {
            m_batteryLevel = lround(total / count);
        }
        else
        {
            m_batteryLevel = TorcPower::UnknownPower;
        }
    }
    else
    {
        m_batteryLevel = TorcPower::ACPower;
    }
    m_httpServiceLock.unlock();

    BatteryUpdated(m_batteryLevel);
}

int TorcPowerUnixDBus::GetBatteryLevel(const QString &Path)
{
    QWriteLocker locker(&m_httpServiceLock);
    QDBusInterface interface(QStringLiteral("org.freedesktop.UPower"), Path, QStringLiteral("org.freedesktop.UPower.Device"),
                             QDBusConnection::systemBus());

    if (interface.isValid())
    {
        QVariant battery = interface.property("IsRechargeable");
        if (battery.isValid() && battery.toBool() == true)
        {
            QVariant percent = interface.property("Percentage");
            if (percent.isValid())
            {
                int result = lround(percent.toFloat() * 100.0);
                return qBound(0, result, 100);
            }
        }
        else
        {
            return TorcPower::ACPower;
        }
    }

    return TorcPower::UnknownPower;
}

void TorcPowerUnixDBus::UpdateProperties(void)
{
    QWriteLocker locker(&m_httpServiceLock);

    m_canSuspend->SetValue(QVariant((bool)false));
    m_canHibernate->SetValue(QVariant((bool)false));
    m_onBattery = false;

    if (m_upowerInterface.isValid())
    {
        QVariant cansuspend = m_upowerInterface.property("CanSuspend");
        if (cansuspend.isValid() && cansuspend.toBool() == true)
            m_canSuspend->SetValue(QVariant((bool)true));

        QVariant canhibernate = m_upowerInterface.property("CanHibernate");
        if (canhibernate.isValid() && canhibernate.toBool() == true)
            m_canHibernate->SetValue(QVariant((bool)true));

        QVariant onbattery = m_upowerInterface.property("OnBattery");
        if (onbattery.isValid() && onbattery.toBool() == true)
            m_onBattery = true;
    }
}

class TorcPowerFactoryUnixDBus : public TorcPowerFactory
{
    void Score(int &Score)
    {
        if (Score <= 10 && TorcPowerUnixDBus::Available())
            Score = 10;
    }

    TorcPower* Create(int Score)
    {
        if (Score <= 10 && TorcPowerUnixDBus::Available())
            return new TorcPowerUnixDBus();

        return nullptr;
    }
} TorcPowerFactoryUnixDBus;
