/* Class TorcNetworkSensors
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torclogging.h"
#include "torcsensors.h"
#include "torcnetworkpwmsensor.h"
#include "torcnetworkswitchsensor.h"
#include "torcnetworksensors.h"

TorcNetworkSensors* TorcNetworkSensors::gNetworkSensors = new TorcNetworkSensors();

TorcNetworkSensors::TorcNetworkSensors()
{
}

TorcNetworkSensors::~TorcNetworkSensors()
{
    //Destroy();
}

void TorcNetworkSensors::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    if (!Details.contains(NETWORK_SENSORS_STRING))
        return;

    QVariantMap::const_iterator i = Details.begin();
    for ( ; i != Details.end(); ++i)
    {
        if (i.key() != NETWORK_SENSORS_STRING)
            continue;

        QVariantMap details = i.value().toMap();
        QVariantMap::const_iterator it = details.begin();
        for ( ; it != details.end(); ++it)
        {
            // iterate over known types
            for (int type = TorcSensor::Unknown; type != TorcSensor::MaxType; type++)
            {
                if (it.key() == TorcSensor::TypeToString(static_cast<TorcSensor::Type>(type)))
                {
                    QVariantMap sensors = it.value().toMap();
                    QVariantMap::iterator it2 = sensors.begin();
                    for ( ; it2 != sensors.end(); ++it2)
                    {
                        QString uniqueid = NETWORK_SENSORS_STRING + "_" + it.key() + "_" + it2.key();

                        if (!TorcDevice::UniqueIdAvailable(uniqueid))
                        {
                            LOG(VB_GENERAL, LOG_WARNING, QString("Already have network sensor named '%1'").arg(uniqueid));
                            continue;
                        }

                        QVariantMap sensor   = it2.value().toMap();
                        QString defaultvalue = sensor.value("default").toString();
                        QString username     = sensor.value("userName").toString();
                        QString userdesc     = sensor.value("userDescription").toString();
                        bool ok = false;
                        double defaultdouble = defaultvalue.toInt(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcSensor* newsensor = NULL;
                        switch (type)
                        {
                            case TorcSensor::PWM:
                                newsensor = new TorcNetworkPWMSensor(defaultdouble, uniqueid);
                                break;
                            case TorcSensor::Switch:
                                newsensor = new TorcNetworkSwitchSensor(defaultdouble, uniqueid);
                                break;
                            default: break;
                        }

                        if (newsensor)
                        {
                            newsensor->SetUserName(username);
                            newsensor->SetUserDescription(userdesc);
                            m_sensors.insert(uniqueid, newsensor);
                        }
                    }
                }
            }
        }
    }
}

void TorcNetworkSensors::Destroy(void)
{
    QMutexLocker locker(m_lock);

    QMap<QString,TorcSensor*>::iterator it = m_sensors.begin();
    for ( ; it != m_sensors.end(); ++it)
    {
        it.value()->DownRef();
        TorcSensors::gSensors->RemoveSensor(it.value());
    }
    m_sensors.clear();
}
