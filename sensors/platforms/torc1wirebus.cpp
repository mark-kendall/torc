/* Class Torc1WireBus
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2014
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
#include <QDir>

// Torc
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcsensors.h"
#include "torc1wirebus.h"
#include "torc1wireds18b20.h"

/*! \class Torc1WireBus
 *  \brief A class to handle the one wire files system (OWFS) for 1Wire devices.
 *
 * Using OWFS, 1Wire devices are presented at /sys/bus/w1/devices. If the kernel modules
 * have not been loaded the /sys/bus/w1 directory will not exist, although it may exist
 * and remain unpopulated if the wire module is still loaded.
 *
 * We no longer monitor the file system and create/remove devices as they are seen. Instead
 * we create those devices specified in the configuration file. Any that are not present
 * will remain invalid.
 *
 * \note Only tested with RaspberryPi under Raspbian; other devices/implementations may
 *       use a different filesystem structure.
 * \note Only the Maxim DS18B20 digital thermometer is currently supported.
 * \note The required kernel modules are not loaded by default on Raspbian. You will
 *       need to 'modprobe' for both w1_gpio and w1_therm and add these to /etc/modules
 *       to ensure they are loaded at each reboot.
*/
Torc1WireBus::Torc1WireBus()
  : TorcDeviceHandler()
{
}

Torc1WireBus::~Torc1WireBus()
{
    Destroy();
}

void Torc1WireBus::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    if (!Details.contains("1Wire"))
        return;

    // check for the correct directory
    QDir dir(ONE_WIRE_DIRECTORY);
    if (!dir.exists(ONE_WIRE_DIRECTORY))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot find 1wire directory (%1) - have you loaded the correct kernel modules").arg(ONE_WIRE_DIRECTORY));
        return;
    }

    QVariantMap wire1 = Details.value("1Wire").toMap();
    QVariantMap::iterator it = wire1.begin();
    for ( ; it != wire1.end(); ++it)
    {
        // TODO move this to a device factory like the I2C bus
        if (!it.key().startsWith("28-"))
            continue;

        QString uniqueid = "1Wire" + it.key();

        // check for uniqueid
        if (!TorcDevice::UniqueIdAvailable(uniqueid))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Device id '%1' already in use - ignoring").arg(uniqueid));
            continue;
        }


        LOG(VB_GENERAL, LOG_INFO, QString("New 1Wire %1 digital thermometer: %2")
            .arg(DS18B20NAME).arg(uniqueid));
        m_sensors.insert(uniqueid, new Torc1WireDS18B20(uniqueid, ONE_WIRE_DIRECTORY + it.key()));
    }
}

void Torc1WireBus::Destroy(void)
{
    QMutexLocker lock(m_lock);

    // delete any extant sensors
    QHash<QString,TorcSensor*>::iterator it = m_sensors.begin();
    for ( ; it != m_sensors.end(); it++)
    {
        TorcSensors::gSensors->RemoveSensor(it.value());
        it.value()->DownRef();
    }
    m_sensors.clear();
}
