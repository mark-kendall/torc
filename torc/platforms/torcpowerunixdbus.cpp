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

        QDBusInterface upower("org.freedesktop.UPower",
                              "/org/freedesktop/UPower",
                              "org.freedesktop.UPower",
                              QDBusConnection::systemBus());
        QDBusInterface consolekit("org.freedesktop.ConsoleKit",
                                  "/org/freedesktop/ConsoleKit/Manager",
                                  "org.freedesktop.ConsoleKit.Manager",
                                  QDBusConnection::systemBus());

        available = upower.isValid() && consolekit.isValid();

        if (!available)
            LOG(VB_GENERAL, LOG_WARNING, "UPower and ConsoleKit not available");
    }

    return available;
}

TorcPowerUnixDBus::TorcPowerUnixDBus()
  : TorcPower(),
    m_onBattery(false),
    m_devices(QMap<QString,int>()),
    m_deviceLock(QMutex::Recursive),
    m_upowerInterface("org.freedesktop.UPower", "/org/freedesktop/UPower", "org.freedesktop.UPower", QDBusConnection::systemBus()),
    m_consoleInterface("org.freedesktop.ConsoleKit", "/org/freedesktop/ConsoleKit/Manager", "org.freedesktop.ConsoleKit.Manager", QDBusConnection::systemBus())
{
    if (m_consoleInterface.isValid())
    {
        QDBusReply<bool> shutdown = m_consoleInterface.call(QLatin1String("CanStop"));
        if (shutdown.isValid() && shutdown.value())
            m_canShutdown->SetValue(QVariant((bool)true));

        QDBusReply<bool> restart = m_consoleInterface.call(QLatin1String("CanRestart"));
        if (restart.isValid() && restart.value())
            m_canRestart->SetValue(QVariant((bool)true));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create ConsoleKit interface");
    }

    // populate devices
    if (m_upowerInterface.isValid())
    {
        QDBusReply<QList<QDBusObjectPath> > devicecheck = m_upowerInterface.call(QLatin1String("EnumerateDevices"));

        if (devicecheck.isValid())
        {
            QMutexLocker locker(&m_deviceLock);

            foreach (QDBusObjectPath device, devicecheck.value())
                m_devices.insert(device.path(), GetBatteryLevel(device.path()));
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to create UPower interface");
    }

    // register for events
    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "Sleeping",
         this, SLOT(Suspending())))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for sleep events");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "Resuming",
         this, SLOT(WokeUp())))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for resume events");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "Changed",
         this, SLOT(Changed())))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for Changed");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "DeviceChanged", "o",
         this, SLOT(DeviceChanged(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceChanged");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "DeviceAdded", "o",
         this, SLOT(DeviceAdded(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceAdded");
    }

    if (!QDBusConnection::systemBus().connect(
         "org.freedesktop.UPower", "/org/freedesktop/UPower",
         "org.freedesktop.UPower", "DeviceRemoved", "o",
         this, SLOT(DeviceRemoved(QDBusObjectPath))))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to register for DeviceRemoved");
    }

    // set battery state
    Changed();

    Debug();
}

TorcPowerUnixDBus::~TorcPowerUnixDBus()
{
}

bool TorcPowerUnixDBus::DoShutdown(void)
{
    if (m_consoleInterface.isValid() && m_canShutdown->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_consoleInterface.callWithCallback(QLatin1String("Stop"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            ShuttingDown();
            return true;
        }

        LOG(VB_GENERAL, LOG_ERR, "Shutdown call failed");
    }

    return false;
}

bool TorcPowerUnixDBus::DoSuspend(void)
{
    if (m_upowerInterface.isValid() && m_canSuspend->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_upowerInterface.callWithCallback(QLatin1String("AboutToSleep"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            if (m_upowerInterface.callWithCallback(QLatin1String("Suspend"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            {
                Suspending();
                return true;
            }
        }

        LOG(VB_GENERAL, LOG_ERR, "Suspend call failed");
    }

    return false;
}

bool TorcPowerUnixDBus::DoHibernate(void)
{
    if (m_upowerInterface.isValid() && m_canHibernate->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_upowerInterface.callWithCallback(QLatin1String("AboutToSleep"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            if (m_upowerInterface.callWithCallback(QLatin1String("Hibernate"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
            {
                Hibernating();
                return true;
            }
        }

        LOG(VB_GENERAL, LOG_ERR, "Hibernate call failed");
    }

    return false;
}

bool TorcPowerUnixDBus::DoRestart(void)
{
    if (m_consoleInterface.isValid() && m_canRestart->GetValue().toBool())
    {
        QList<QVariant> dummy;
        if (m_consoleInterface.callWithCallback(QLatin1String("Restart"), dummy, (QObject*)this, SLOT(DBusCallback()), SLOT(DBusError(QDBusError))))
        {
            Restarting();
            return true;
        }

        LOG(VB_GENERAL, LOG_ERR, "Restart call failed");
    }

    return false;
}

void TorcPowerUnixDBus::DeviceAdded(QDBusObjectPath Device)
{
    {
        QMutexLocker locker(&m_deviceLock);

        if (m_devices.contains(Device.path()))
            return;

        m_devices.insert(Device.path(), GetBatteryLevel(Device.path()));
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Added UPower.Device '%1'")
        .arg(Device.path()));

    UpdateBattery();
}

void TorcPowerUnixDBus::DeviceRemoved(QDBusObjectPath Device)
{
    {
        QMutexLocker locker(&m_deviceLock);

        if (!m_devices.contains(Device.path()))
            return;

        m_devices.remove(Device.path());
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Removed UPower.Device '%1'")
        .arg(Device.path()));

    UpdateBattery();
}

void TorcPowerUnixDBus::DeviceChanged(QDBusObjectPath Device)
{
    {
        QMutexLocker locker(&m_deviceLock);

        if (!m_devices.contains(Device.path()))
            return;

        m_devices[Device.path()] = GetBatteryLevel(Device.path());
    }

    UpdateBattery();
}

void TorcPowerUnixDBus::DBusError(QDBusError Error)
{
    LOG(VB_GENERAL, LOG_ERR, QString("DBus callback error: %1, %2")
        .arg(Error.name()).arg(Error.message().trimmed()));
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
    if (m_onBattery)
    {
        QMutexLocker locker(&m_deviceLock);

        qreal total = 0;
        int   count = 0;

        QMap<QString,int>::iterator it = m_devices.begin();
        for ( ; it != m_devices.end(); ++it)
        {
            if (it.value() >= 0 && it.value() <= 100)
            {
                count++;
                total += (qreal)it.value();
            }
        }

        if (count > 0)
        {
            m_batteryLevel = (int)((total / count) + 0.5);
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

    BatteryUpdated(m_batteryLevel);
}

int TorcPowerUnixDBus::GetBatteryLevel(const QString &Path)
{
    QDBusInterface interface("org.freedesktop.UPower", Path, "org.freedesktop.UPower.Device",
                             QDBusConnection::systemBus());

    if (interface.isValid())
    {
        QVariant battery = interface.property("IsRechargeable");
        if (battery.isValid() && battery.toBool() == true)
        {
            QVariant percent = interface.property("Percentage");
            if (percent.isValid())
            {
                int result = percent.toFloat() * 100.0;
                if (result >= 0.0 && result <= 100.0)
                    return (int)(result + 0.5);
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

        return NULL;
    }
} TorcPowerFactoryUnixDBus;
