/* Class TorcNetworkInputs
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2015-18
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
#include "torcinputs.h"
#include "torcnetworkpwmoutput.h"
#include "torcnetworkswitchoutput.h"
#include "torcnetworktemperatureoutput.h"
#include "torcnetworkphoutput.h"
#include "torcnetworkbuttonoutput.h"
#include "torcoutputs.h"
#include "torcnetworkpwminput.h"
#include "torcnetworkswitchinput.h"
#include "torcnetworktemperatureinput.h"
#include "torcnetworkphinput.h"
#include "torcnetworkbuttoninput.h"
#include "torcnetworkinputs.h"

/*! \class TorcNetworkInputs
 *
 * Creates and manages known network (i.e. user set) sensors.
 *
 * \code
 *
 * <torc>
 *     <inputs or outputs>
 *         <network>
 *             <switch or pwm or ph etc>
 *                 <name></name>
 *                 <default></default>
 *                 <username></username>
 *                 <userdescription></userdescription>
 *             </switch etc>
 *         </network>
 *     </inputs or outputs>
 * </torc>
 *
 * \endcode
*/
TorcNetworkInputs* TorcNetworkInputs::gNetworkInputs = new TorcNetworkInputs();

TorcNetworkInputs::TorcNetworkInputs()
{
}

TorcNetworkInputs::~TorcNetworkInputs()
{
    //Destroy();
}

void TorcNetworkInputs::Create(const QVariantMap &Details)
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
            if (j.key() != NETWORK_INPUTS_STRING)
                continue;

            QVariantMap details = j.value().toMap();
            QVariantMap::const_iterator it = details.constBegin();
            for ( ; it != details.constEnd(); ++it)
            {
                // iterate over known types <pwm>, <switch> etc
                for (int type = TorcInput::Unknown; type != TorcInput::MaxType; type++)
                {
                    if (it.key() == TorcInput::TypeToString(static_cast<TorcInput::Type>(type)))
                    {
                        QVariantMap sensor   = it.value().toMap();
                        QString defaultvalue = sensor.value("default").toString();
                        QString uniqueid     = sensor.value("name").toString(); // ugly... assumes it is present

                        bool ok = false;
                        double defaultdouble = defaultvalue.toDouble(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcInput* newsensor = NULL;
                        TorcOutput* newoutput = NULL;
                        switch (type)
                        {
                            case TorcInput::PWM:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkPWMInput(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkPWMOutput(defaultdouble, sensor);
                                }
                                break;
                            case TorcInput::Switch:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkSwitchInput(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkSwitchOutput(defaultdouble, sensor);
                                }
                                break;
                            case TorcInput::Temperature:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkTemperatureInput(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkTemperatureOutput(defaultdouble, sensor);
                                }
                                break;
                            case TorcInput::pH:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkpHInput(defaultdouble, sensor);
                                    else
                                        newoutput = new TorcNetworkpHOutput(defaultdouble, sensor);
                                }
                            case TorcInput::Button:
                                {
                                    if (issensor)
                                        newsensor = new TorcNetworkButtonInput(defaultdouble, sensor);
                                    else
                                    {
                                        newoutput = new TorcNetworkButtonOutput(defaultdouble, sensor);
                                    }
                                }
                            default: break;
                        }

                        if (newsensor)
                            m_inputs.insert(uniqueid, newsensor);
                        if (newoutput)
                            m_outputs.insert(uniqueid, newoutput);
                    }
                }
            }
        }
    }
}

void TorcNetworkInputs::Destroy(void)
{
    QMutexLocker locker(m_lock);

    QMap<QString,TorcInput*>::iterator it = m_inputs.begin();
    for ( ; it != m_inputs.end(); ++it)
    {
        it.value()->DownRef();
        TorcInputs::gInputs->RemoveInput(it.value());
    }
    m_inputs.clear();

    QMap<QString,TorcOutput*>::iterator it2 = m_outputs.begin();
    for ( ; it2 != m_outputs.end(); ++it2)
    {
        it2.value()->DownRef();
        TorcOutputs::gOutputs->RemoveOutput(it2.value());
    }
    m_outputs.clear();
}
