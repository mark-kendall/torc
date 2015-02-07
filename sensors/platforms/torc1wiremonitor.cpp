/* Class Torc1WireMonitor
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
#include <QDirIterator>

// Torc
#include "torclogging.h"
#include "torcadminthread.h"
#include "torcsensors.h"
#include "torc1wiremonitor.h"
#include "torc1wireds18b20.h"

/*! \class Torc1WireMonitor
 *  \brief A class to monitor the one wire files system (OWFS) for 1Wire devices.
 *
 * Using OWFS, 1Wire devices are presented at /sys/bus/w1/devices. If the kernel modules
 * have not been loaded the /sys/bus/w1 directory will not exist, although it may exist
 * and remain unpopulated if the wire module is still loaded.
 *
 * In short, we monitor for the existence of /sys/bus/w1/devices and examine the directory
 * structure for known models.
 *
 * \note Only tested with RaspberryPi under Raspbian; other devices/implementations may
 *       use a different filesystem structure.
 * \note Only the Maxim DS18B20 digital thermometer is currently supported.
 * \note The required kernel modules are not loaded by default on Raspbian. You will
 *       need to 'modprobe' for both w1_gpio and w1_therm and add these to /etc/modules
 *       to ensure they are loaded at each reboot.
*/
Torc1WireMonitor::Torc1WireMonitor()
  : QObject(),
    m_timer(new QTimer()),
    m_1wireDirectory(new QFileSystemWatcher(this))
{
    // set up the timer
    m_timer->setTimerType(Qt::CoarseTimer);
    // retry every 10 seconds
    m_timer->setInterval(10000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(Start()));

    // ensure we receive directory updates
    connect(m_1wireDirectory, SIGNAL(directoryChanged(QString)), this, SLOT(DirectoryChanged(QString)));

    // kick off
    Start();
}

Torc1WireMonitor::~Torc1WireMonitor()
{
    delete m_timer;
    delete m_1wireDirectory;

    // delete any extant sensors
    QHash<QString,TorcSensor*>::iterator it = m_sensors.begin();
    for ( ; it != m_sensors.end(); it++)
        it.value()->DownRef();
    m_sensors.clear();
}

void Torc1WireMonitor::Start(void)
{
    // already monitoring?
    if (!m_1wireDirectory->directories().isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Start called when already monitoring");
        // stop the timer
        m_timer->stop();
        return;
    }

    if (m_1wireDirectory->addPath(ONE_WIRE_DIRECTORY))
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Started monitoring %1").arg(ONE_WIRE_DIRECTORY));
        // don't restart
        m_timer->stop();
        // initial scan
        DirectoryChanged(QString(""));
        return;
    }

    // we failed to monitor the directory - probably isn't there.
    // schedule a retry in case there's a little modprobe'ing
    LOG(VB_GENERAL, LOG_INFO, QString("Failed to monitor %1").arg(ONE_WIRE_DIRECTORY));
    ScheduleRetry();
}

/*! \brief Process directory and/or contents changes
*/
void Torc1WireMonitor::DirectoryChanged(const QString&)
{
    // was the directory removed
    if (m_1wireDirectory->directories().isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("One wire directory %1 went away").arg(ONE_WIRE_DIRECTORY));
        ScheduleRetry();
        return;
    }

    QStringList devices;
    QDirIterator it(ONE_WIRE_DIRECTORY, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
    if (it.hasNext())
    {
        while (it.hasNext())
        {
            it.next();
            QString name = it.fileName();

            // is it a recognised device type?
            if (!name.startsWith("28-"))
                continue;

            // build the list of known devices.
            devices.append(name);

            // is it new?
            if (!m_sensors.contains(name))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("New 1Wire %1 digital thermometer: %2")
                    .arg(DS18B20NAME).arg(it.fileName()));
                m_sensors.insert(name, new Torc1WireDS18B20(it.fileName()));
            }
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, "No 1Wire devices detected.");
    }

    // remove devices
    QMutableHashIterator<QString,TorcSensor*> i(m_sensors);
    while (i.hasNext())
    {
        i.next();
        if (!devices.contains(i.key()))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("1Wire device %1 removed").arg(i.key()));
            TorcSensor* old = i.value();
            i.remove();
            old->DownRef();
            // NB we need to unregister the sensor here to ensure reference counting works
            // and the sensor's destructor is called.
            TorcSensors::gSensors->RemoveSensor(old);
        }
    }
}

void Torc1WireMonitor::ScheduleRetry(void)
{
    if (!m_timer->isActive())
        m_timer->start();
}

static class OneWireMonitorObject : public TorcAdminObject
{
  public:
    OneWireMonitorObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY), // must start AFTER TorcSensors
        monitor(NULL)
    {
    }

    void Create(void)
    {
        monitor = new Torc1WireMonitor();
    }

    void Destroy(void)
    {
        delete monitor;
        monitor = NULL;
    }

  private:
    Torc1WireMonitor *monitor;
} OneWireMonitorObject;
