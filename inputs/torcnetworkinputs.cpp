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
#include "torccoreutils.h"
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
#include "torcnetworkintegerinput.h"
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
    QWriteLocker locker(&m_handlerLock);

    QVariantMap::const_iterator i = Details.constBegin();
    for ( ; i != Details.constEnd(); ++i)
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
                    // FIXME using TorcInput::Type to validate both inputs and outputs - will break at some point
                    if (it.key() == TorcCoreUtils::EnumToLowerString<TorcInput::Type>(static_cast<TorcInput::Type>(type)))
                    {
                        QVariantMap input    = it.value().toMap();
                        QString defaultvalue = network ? input.value(QStringLiteral("default")).toString() : input.value(QStringLiteral("value")).toString();
                        QString uniqueid     = input.value(QStringLiteral("name")).toString(); // ugly... assumes it is present

                        bool ok = false;
                        double defaultdouble = defaultvalue.toDouble(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcInput*  newinput  = nullptr;
                        TorcOutput* newoutput = nullptr;
                        switch (type)
                        {
                            case TorcInput::PWM:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkPWMInput(defaultdouble, input) : new TorcPWMInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::PWM), input);
                                    else
                                        newoutput = network ? new TorcNetworkPWMOutput(defaultdouble, input) : new TorcPWMOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::PWM), input);
                                }
                                break;
                            case TorcInput::Switch:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkSwitchInput(defaultdouble, input) : new TorcSwitchInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Switch), input);
                                    else
                                        newoutput = network ? new TorcNetworkSwitchOutput(defaultdouble, input) : new TorcSwitchOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::Switch), input);
                                }
                                break;
                            case TorcInput::Temperature:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkTemperatureInput(defaultdouble, input) : new TorcTemperatureInput(defaultdouble, -1000, 1000, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Temperature), input);
                                    else
                                        newoutput = network ? new TorcNetworkTemperatureOutput(defaultdouble, input) : new TorcTemperatureOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::Temperature), input);
                                }
                                break;
                            case TorcInput::pH:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkpHInput(defaultdouble, input) : new TorcpHInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::pH), input);
                                    else
                                        newoutput = network ? new TorcNetworkpHOutput(defaultdouble, input) : new TorcpHOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::pH), input);
                                }
                                break;
                            case TorcInput::Integer:
                                {
                                    if (isinput)
                                        newinput = network ? new TorcNetworkIntegerInput(defaultdouble, input) : new TorcIntegerInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Integer), input);
                                    //else
                                    //    newoutput = network ? new TorcNetworkIntegerOutput(defaultdouble, input) : new TorcIntegerOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::Integer), input);
                                }
                                break;
                            case TorcInput::Button:
                                {
                                    if (constant)
                                    {
                                        // this should be flagged by the XSD
                                        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot create constant button input"));
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
    QWriteLocker locker(&m_handlerLock);

    QMap<QString,TorcInput*>::const_iterator it = m_inputs.constBegin();
    for ( ; it != m_inputs.constEnd(); ++it)
    {
        it.value()->DownRef();
        TorcInputs::gInputs->RemoveInput(it.value());
    }
    m_inputs.clear();

    QMap<QString,TorcOutput*>::const_iterator it2 = m_outputs.constBegin();
    for ( ; it2 != m_outputs.constEnd(); ++it2)
    {
        it2.value()->DownRef();
        TorcOutputs::gOutputs->RemoveOutput(it2.value());
    }
    m_outputs.clear();
}
