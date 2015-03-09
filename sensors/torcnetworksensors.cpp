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
#include "torcnetworkpwmoutput.h"
#include "torcnetworkswitchoutput.h"
#include "torcoutputs.h"
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
                        QVariantMap sensor   = it2.value().toMap();
                        QString defaultvalue = sensor.value("default").toString();
                        QString state        = sensor.value("state").toString().toUpper();
                        QString uniqueid     = sensors.value("name").toString(); // ugly... assumes it is present

                        if (state != "INPUT" && state != "OUTPUT")
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Unknown network device type '%1' for device '%2'").arg(state).arg(uniqueid));
                            continue;
                        }

                        bool issensor = (state == "INPUT");
                        bool ok = false;
                        double defaultdouble = defaultvalue.toInt(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcSensor* newsensor = NULL;
                        TorcOutput* newoutput = NULL;
                        switch (type)
                        {
                            case TorcSensor::PWM:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkPWMSensor(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkPWMOutput(defaultdouble, sensor);
                                }
                                break;
                            case TorcSensor::Switch:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkSwitchSensor(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkSwitchOutput(defaultdouble, sensor);
                                }
                                break;
                            default: break;
                        }

                        if (newsensor)
                            m_sensors.insert(uniqueid, newsensor);
                        if (newoutput)
                            m_outputs.insert(uniqueid, newoutput);
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

    QMap<QString,TorcOutput*>::iterator it2 = m_outputs.begin();
    for ( ; it2 != m_outputs.end(); ++it2)
    {
        it2.value()->DownRef();
        TorcOutputs::gOutputs->RemoveOutput(it2.value());
    }
    m_outputs.clear();
}
