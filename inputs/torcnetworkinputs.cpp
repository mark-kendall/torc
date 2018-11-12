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
 * Creates and manages known network (i.e. user set) sensors (actually inputs and outputs)
 * as well as static/constant inputs and outputs.
 *
 * \code
 *
 * <torc>
 *   <inputs or outputs>
 *     <network>
 *       <switch/pwm/temperature/ph/button>
 *         <name></name>
 *         <default></default>
 *         <username></username>
 *         <userdescription></userdescription>
 *       </switch etc>
 *     </network>
 *     <constant>
 *       <switch/pwm/temperature/ph> -- NO BUTTON
 *         <name></name>
 *         <default></default>
 *         <username></username>
 *         <userdescription></userdescription>
 *       </switch etc>
 *     </constant>
 *   </inputs or outputs>
 * </torc>
 *
 * \endcode
*/
TorcNetworkInputs* TorcNetworkInputs::gNetworkInputs = new TorcNetworkInputs();

TorcNetworkInputs::TorcNetworkInputs()
  : TorcDeviceHandler(),
    m_inputs(),
    m_outputs()
{
}

void TorcNetworkInputs::Create(const QVariantMap &Details)
{
    QMutexLocker locker(&m_lock);

    QVariantMap::const_iterator i = Details.begin();
    for ( ; i != Details.end(); ++i)
    {
        // network devices can be <inputs> or <outputs>
        if ((i.key() != INPUTS_DIRECTORY) && (i.key() != OUTPUTS_DIRECTORY))
            continue;

        bool isinput = (i.key() == INPUTS_DIRECTORY);

        QVariantMap devices = i.value().toMap();
        QVariantMap::const_iterator j = devices.constBegin();
        for ( ; j != devices.constEnd(); ++j)
        {
            // look for <network> or <constant>
            QString group = j.key();
            bool network  = group == NETWORK_INPUTS_STRING;
            bool constant = group == CONSTANT_INPUTS_STRING;

            if (!network && !constant)
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
                        QVariantMap input    = it.value().toMap();
                        QString defaultvalue = network ? input.value("default").toString() : input.value("value").toString();
                        QString uniqueid     = input.value("name").toString(); // ugly... assumes it is present

                        bool ok = false;
                        double defaultdouble = defaultvalue.toDouble(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcInput*  newinput  = NULL;
                        TorcOutput* newoutput = NULL;
                        switch (type)
                        {
                            case TorcInput::PWM:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkPWMInput(defaultdouble, input) : new TorcPWMInput(defaultdouble, "ConstantPWM", input);
                                    else
                                        newoutput = network ? new TorcNetworkPWMOutput(defaultdouble, input) : new TorcPWMOutput(defaultdouble, "ConstantPWM", input);
                                }
                                break;
                            case TorcInput::Switch:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkSwitchInput(defaultdouble, input) : new TorcSwitchInput(defaultdouble, "ConstantSwitch", input);
                                    else
                                        newoutput = network ? new TorcNetworkSwitchOutput(defaultdouble, input) : new TorcSwitchOutput(defaultdouble, "ConstantSwitch", input);
                                }
                                break;
                            case TorcInput::Temperature:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkTemperatureInput(defaultdouble, input) : new TorcTemperatureInput(defaultdouble, -1000, 1000, "ConstantTemp", input);
                                    else
                                        newoutput = network ? new TorcNetworkTemperatureOutput(defaultdouble, input) : new TorcTemperatureOutput(defaultdouble, "ConstantTemp", input);
                                }
                                break;
                            case TorcInput::pH:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkpHInput(defaultdouble, input) : new TorcpHInput(defaultdouble, "ConstantpH", input);
                                    else
                                        newoutput = network ? new TorcNetworkpHOutput(defaultdouble, input) : new TorcpHOutput(defaultdouble, "ConstantpH", input);
                                }
                                break;
                            case TorcInput::Button:
                                {
                                    if (constant)
                                    {
                                        // this should be flagged by the XSD
                                        LOG(VB_GENERAL, LOG_ERR, "Cannot create constant button input");
                                    }
                                    else
                                    {
                                        if (isinput)
                                            newinput = new TorcNetworkButtonInput(defaultdouble, input);
                                        else
                                        {
                                            newoutput = new TorcNetworkButtonOutput(defaultdouble, input);
                                        }
                                    }
                                }
                            default: break;
                        }

                        if (newinput)
                            m_inputs.insert(uniqueid, newinput);
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
    QMutexLocker locker(&m_lock);

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
