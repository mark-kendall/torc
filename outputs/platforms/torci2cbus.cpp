/* Class TorcI2CBus
*
* Copyright (C) Mark Kendall 2015
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

// Torc
#include "torclogging.h"
#include "torci2cbus.h"

TorcI2CDevice::TorcI2CDevice(int Address)
  : m_address(Address),
    m_fd(-1)
{
}

TorcI2CDevice::~TorcI2CDevice()
{
}

TorcI2CDeviceFactory* TorcI2CDeviceFactory::gTorcI2CDeviceFactory = NULL;

TorcI2CDeviceFactory::TorcI2CDeviceFactory()
{
    nextTorcI2CDeviceFactory = gTorcI2CDeviceFactory;
    gTorcI2CDeviceFactory = this;
}

TorcI2CDeviceFactory::~TorcI2CDeviceFactory()
{
}

TorcI2CDeviceFactory* TorcI2CDeviceFactory::GetTorcI2CDeviceFactory(void)
{
    return gTorcI2CDeviceFactory;
}

TorcI2CDeviceFactory* TorcI2CDeviceFactory::NextFactory(void) const
{
    return nextTorcI2CDeviceFactory;
}

TorcI2CBus* TorcI2CBus::gTorcI2CBus = new TorcI2CBus();

TorcI2CBus::TorcI2CBus()
  : TorcDeviceHandler()
{
}

TorcI2CBus::~TorcI2CBus()
{
    Destroy();
}

void TorcI2CBus::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    if (Details.contains("I2C"))
    {
        QVariantMap i2c = Details.value("I2C").toMap();
        QVariantMap::iterator it = i2c.begin();
        for ( ; it != i2c.end(); ++it)
        {
            bool ok = false;
            // N.B. assumes hexadecimal 0xXX
            int address = it.key().toInt(&ok, 16);
            if (!ok)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse I2C address '%1'").arg(it.key()));
                continue;
            }

            if (m_devices.contains(address))
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("I2C bus already contains device at 0x%1 - ignoring").arg(address, 0, 16));
                continue;
            }

            QVariantMap details = it.value().toMap();
            if (!details.contains("model"))
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("I2C device at 0x%1 does not give model - ignoring").arg(address, 0, 16));
                continue;
            }

            QString model = details.value("model").toString();
            TorcI2CDevice* device = NULL;
            TorcI2CDeviceFactory* factory = TorcI2CDeviceFactory::GetTorcI2CDeviceFactory();
            for ( ; factory; factory = factory->NextFactory())
            {
                device = factory->Create(address, model, details);
                if (device)
                    break;
            }

            if (device)
                m_devices.insert(address, device);
            else
                LOG(VB_GENERAL, LOG_WARNING, QString("Unable to find handler for I2C device '%1'").arg(model));
        }
    }
}

void TorcI2CBus::Destroy(void)
{
    QMutexLocker locker(m_lock);

    qDeleteAll(m_devices);
    m_devices.clear();
}
