/* Class TorcInputs
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2014-18
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
#include "torcadminthread.h"
#include "torccentral.h"
#include "torcnetworkpwminput.h"
#include "torcnetworkswitchinput.h"
#include "torcnetworktemperatureinput.h"
#include "torcnetworkphinput.h"
#include "torcnetworkbuttoninput.h"
#include "torcnetworkintegerinput.h"
#include "torcinputs.h"

#define BLACKLIST QStringLiteral("")

TorcInputs* TorcInputs::gInputs = new TorcInputs();

/*! \brief TorcInputs contains the master record of known inputs.
 *
 * A static global TorcInputs object is created (this could alternatively be created
 * as a TorcAdminObject). This object will list these inputs as an HTTP service.
 *
 * Any subclass of TorcInput will automatically register itself on creation. Any class
 * creating inputs must DownRef AND call RemoveInput when deleting them.
 *
 * It also creates and manages known network (i.e. user set) and constant inputs.
 *
 * \code
 *
 * <torc>
 *   <inputs>
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
 *   </inputs>
 * </torc>
 *
 * \endcode
 *
 * \note This class is thread safe.
*/
TorcInputs::TorcInputs()
  : QObject(),
    TorcHTTPService(this, INPUTS_DIRECTORY, QStringLiteral("inputs"), TorcInputs::staticMetaObject, BLACKLIST),
    TorcDeviceHandler(),
    inputList(),
    inputTypes(),
    m_createdInputs()
{
}

QString TorcInputs::GetUIName(void)
{
    return tr("Inputs");
}

void TorcInputs::Graph(QByteArray *Data)
{
    if (!Data)
        return;

    QString start =
          QStringLiteral("    subgraph cluster_0 {\r\n"
                         "        label = \"%1\";\r\n"
                         "        style=filled;\r\n"
                         "        fillcolor = \"grey95\";\r\n").arg(tr("Inputs"));
    Data->append(start);

    foreach(TorcInput *input, inputList)
    {
        QString id    = input->GetUniqueId();
        QString label = input->GetUserName();
        QString desc;
        QStringList source = input->GetDescription();
        foreach (const QString &item, source)
            if (!item.isEmpty())
                desc.append(QString(DEVICE_LINE_ITEM).arg(item));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Default %1").arg(input->GetDefaultValue())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Valid %1").arg(input->GetValid())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Value %1").arg(input->GetValue())));

        if (label.isEmpty())
            label = id;
        Data->append(QStringLiteral("        \"%1\" [shape=record id=\"%1\" label=<<B>%2</B>%3>];\r\n").arg(id, label, desc));
    }

    Data->append("    }\r\n\r\n");
}

void TorcInputs::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Return a map of known inputs.
 *
 * To simplify javascript on the webclient side, the map is presented as a list of input paths
 * by type, allowing a client to quickly filter and subscribe to the individual services for
 * further detail.
*/
QVariantMap TorcInputs::GetInputList(void)
{
    QVariantMap result;
    QReadLocker locker(&m_httpServiceLock);

    // iterate over our list for each input type
    for (int type = TorcInput::Unknown; type < TorcInput::MaxType; type++)
    {
        QStringList inputsfortype;
        foreach (TorcInput *input, inputList)
            if (input->GetType() == type)
                inputsfortype.append(input->GetUniqueId());

        if (!inputsfortype.isEmpty())
            result.insert(TorcCoreUtils::EnumToLowerString<TorcInput::Type>(static_cast<TorcInput::Type>(type)), inputsfortype);
    }
    return result;
}

QStringList TorcInputs::GetInputTypes(void)
{
    return TorcCoreUtils::EnumList<TorcInput::Type>();
}

void TorcInputs::AddInput(TorcInput *Input)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (!Input)
        return;

    if (inputList.contains(Input))
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Already have an input named %1 - ignoring").arg(Input->GetUniqueId()));
        return;
    }

    Input->UpRef();
    inputList.append(Input);
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Registered input '%1'").arg(Input->GetUniqueId()));
    emit InputsChanged();
}

void TorcInputs::RemoveInput(TorcInput *Input)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (!Input)
        return;

    if (!inputList.contains(Input))
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Input %1 not recognised - cannot remove").arg(Input->GetUniqueId()));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Input %1 de-registered").arg(Input->GetUniqueId()));
    Input->DownRef();
    inputList.removeOne(Input);
    emit InputsChanged();
}

void TorcInputs::Create(const QVariantMap &Details)
{
    QWriteLocker locker(&m_handlerLock);

    QVariantMap::const_iterator i = Details.constBegin();
    for ( ; i != Details.constEnd(); ++i)
    {
        if (i.key() != INPUTS_DIRECTORY)
            continue;

        QVariantMap devices = i.value().toMap();
        QVariantMap::const_iterator j = devices.constBegin();
        for ( ; j != devices.constEnd(); ++j)
        {
            // look for <network> or <constant>
            QString group = j.key();
            bool network  = group == NETWORK_DEVICE_STRING;
            bool constant = group == CONSTANT_DEVICE_STRING;

            if (!network && !constant)
                continue;

            QVariantMap details = j.value().toMap();
            QVariantMap::const_iterator it = details.constBegin();
            for ( ; it != details.constEnd(); ++it)
            {
                // iterate over known types <pwm>, <switch> etc
                for (int type = TorcInput::Unknown; type != TorcInput::MaxType; type++)
                {
                    if (it.key() == TorcCoreUtils::EnumToLowerString<TorcInput::Type>(static_cast<TorcInput::Type>(type)))
                    {
                        QVariantMap input    = it.value().toMap();
                        QString defaultvalue = network ? input.value(QStringLiteral("default")).toString() : input.value(QStringLiteral("value")).toString();
                        QString uniqueid     = input.value(QStringLiteral("name"), QStringLiteral("Error")).toString();

                        bool ok = false;
                        double defaultdouble = defaultvalue.toDouble(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcInput*  newinput  = nullptr;
                        switch (type)
                        {
                            case TorcInput::PWM:
                                newinput = network ? new TorcNetworkPWMInput(defaultdouble, input) : new TorcPWMInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::PWM), input);
                                break;
                            case TorcInput::Switch:
                                newinput = network ? new TorcNetworkSwitchInput(defaultdouble, input) : new TorcSwitchInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Switch), input);
                                break;
                            case TorcInput::Temperature:
                                newinput = network ? new TorcNetworkTemperatureInput(defaultdouble, input) : new TorcTemperatureInput(defaultdouble, -1000, 1000, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Temperature), input);
                                break;
                            case TorcInput::pH:
                                newinput = network ? new TorcNetworkpHInput(defaultdouble, input) : new TorcpHInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::pH), input);
                                break;
                            case TorcInput::Integer:
                                newinput = network ? new TorcNetworkIntegerInput(defaultdouble, input) : new TorcIntegerInput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Integer), input);
                                break;
                            case TorcInput::Button:
                                if (constant)
                                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot create constant button input"));
                                else
                                    newinput = new TorcNetworkButtonInput(defaultdouble, input);
                            default: break;
                        }

                        if (newinput)
                            m_createdInputs.insert(uniqueid, newinput);
                    }
                }
            }
        }
    }
}

void TorcInputs::Destroy()
{
    QWriteLocker locker(&m_handlerLock);

    QMap<QString,TorcInput*>::const_iterator it = m_createdInputs.constBegin();
    for ( ; it != m_createdInputs.constEnd(); ++it)
    {
        it.value()->DownRef();
        RemoveInput(it.value());
    }
    m_createdInputs.clear();
}
