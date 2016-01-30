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
#include "torcnetworktemperatureoutput.h"
#include "torcnetworkphoutput.h"
#include "torcnetworkbuttonoutput.h"
#include "torcoutputs.h"
#include "torcnetworkpwmsensor.h"
#include "torcnetworkswitchsensor.h"
#include "torcnetworktemperaturesensor.h"
#include "torcnetworkphsensor.h"
#include "torcnetworkbuttonsensor.h"
#include "torcnetworksensors.h"

/*! \class TorcNetworkSensors
 *
 * Creates and manages known sensors.
 *
 * \code
 *
 * <torc>
 *     <sensors or outputs>
 *         <network>
 *             <switch or pwm or ph etc>
 *                 <name></name>
 *                 <default></default>
 *                 <username></username>
 *                 <userdescription></userdescription>
 *             </switch etc>
 *         </network>
 *     </sensors>
 * </torc>
 *
 * \endcode
*/
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

    QVariantMap::const_iterator i = Details.begin();
    for ( ; i != Details.end(); ++i)
    {
        // network devices can be <sensors> or <outputs>
        if ((i.key() != SENSORS_DIRECTORY) && (i.key() != OUTPUTS_DIRECTORY))
            continue;

        bool issensor = (i.key() == SENSORS_DIRECTORY);

        QVariantMap devices = i.value().toMap();
        QVariantMap::const_iterator j = devices.constBegin();
        for ( ; j != devices.constEnd(); j++)
        {
            // look for <network>
            if (j.key() != NETWORK_SENSORS_STRING)
                continue;

            QVariantMap details = j.value().toMap();
            QVariantMap::const_iterator it = details.constBegin();
            for ( ; it != details.constEnd(); ++it)
            {
                // iterate over known types <pwm>, <switch> etc
                for (int type = TorcSensor::Unknown; type != TorcSensor::MaxType; type++)
                {
                    if (it.key() == TorcSensor::TypeToString(static_cast<TorcSensor::Type>(type)))
                    {
                        QVariantMap sensor   = it.value().toMap();
                        QString defaultvalue = sensor.value("default").toString();
                        QString uniqueid     = sensor.value("name").toString(); // ugly... assumes it is present

                        bool ok = false;
                        double defaultdouble = defaultvalue.toDouble(&ok);
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
                            case TorcSensor::Temperature:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkTemperatureSensor(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkTemperatureOutput(defaultdouble, sensor);
                                }
                                break;
                            case TorcSensor::pH:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkpHSensor(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkpHOutput(defaultdouble, sensor);
                                }
                            case TorcSensor::Button:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkButtonSensor(defaultdouble, sensor);
                                    else
                                    {
                                        newoutput = new TorcNetworkButtonOutput(defaultdouble, sensor);
                                    }
                                }
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
