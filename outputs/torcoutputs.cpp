/* Class TorcOutput
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
#include "torccentral.h"
#include "torcnetworkpwmoutput.h"
#include "torcnetworkswitchoutput.h"
#include "torcnetworktemperatureoutput.h"
#include "torcnetworkphoutput.h"
#include "torcnetworkbuttonoutput.h"
#include "torcoutputs.h"

#define BLACKLIST QStringLiteral("")

TorcOutputs* TorcOutputs::gOutputs = new TorcOutputs();

/*! \brief TorcOutputs contains the master record of known outputs.
 *
 * A static global TorcOutputs object is created (this could alternatively be created
 * as a TorcAdminObject). This object will list these outputs as an HTTP service.
 *
 * Any subclass of TorcOutput will automatically register itself on creation. Any class
 * creating outputs must DownRef AND call RemoveOutput when deleting them.
 *
 * It also creates and manages known network (i.e. user set) and constant outputs.
 *
 * \code
 *
 * <torc>
 *   <outputs>
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
 *   </outputs>
 * </torc>
 *
 * \endcode
 *
 * \note This class is thread safe.
*/
TorcOutputs::TorcOutputs()
  : QObject(),
    TorcHTTPService(this, OUTPUTS_DIRECTORY, QStringLiteral("outputs"), TorcOutputs::staticMetaObject, BLACKLIST),
    TorcDeviceHandler(),
    outputList(),
    outputTypes(),
    m_createdOutputs()
{
}

QString TorcOutputs::GetUIName(void)
{
    return tr("Outputs");
}

void TorcOutputs::Graph(QByteArray* Data)
{
    if (!Data)
        return;

    QString start =
         QStringLiteral("    subgraph cluster_2 {\r\n"
                        "        label = \"%1\";\r\n"
                        "        style=filled;\r\n"
                        "        fillcolor=\"grey95\";\r\n").arg(tr("Outputs"));
    Data->append(start);
    foreach(TorcOutput* output, outputList)
        output->Graph(Data);
    Data->append("    }\r\n\r\n");
}

void TorcOutputs::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Return a map of known outputs.
*/
QVariantMap TorcOutputs::GetOutputList(void)
{
    QVariantMap result;
    QReadLocker locker(&m_httpServiceLock);

    // iterate over our list for each output type
    for (int type = TorcOutput::Unknown; type < TorcOutput::MaxType; type++)
    {
        QStringList outputsfortype;
        foreach (TorcOutput *output, outputList)
            if (output->GetType() == type)
                outputsfortype.append(output->GetUniqueId());

        if (!outputsfortype.isEmpty())
            result.insert(TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(static_cast<TorcOutput::Type>(type)), outputsfortype);
    }
    return result;
}

QStringList TorcOutputs::GetOutputTypes(void)
{
    return TorcCoreUtils::EnumList<TorcOutput::Type>();
}

void TorcOutputs::AddOutput(TorcOutput *Output)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (!Output)
        return;

    if (Output->GetUniqueId().isEmpty())
        return;

    if (outputList.contains(Output))
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Already have output named %1 - ignoring").arg(Output->GetUniqueId()));
        return;
    }

    Output->UpRef();
    outputList.append(Output);
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Registered output %1").arg(Output->GetUniqueId()));
    emit OutputsChanged();
}

void TorcOutputs::RemoveOutput(TorcOutput *Output)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (!Output)
        return;

    if (Output->GetUniqueId().isEmpty())
        return;

    if (!outputList.contains(Output))
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Output %1 not recognised - cannot remove").arg(Output->GetUniqueId()));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Output %1 deregistered").arg(Output->GetUniqueId()));
    Output->DownRef();
    outputList.removeOne(Output);
    emit OutputsChanged();
}

void TorcOutputs::Create(const QVariantMap &Details)
{
    QWriteLocker locker(&m_handlerLock);

    QVariantMap::const_iterator i = Details.constBegin();
    for ( ; i != Details.constEnd(); ++i)
    {
        // network devices can be <inputs> or <outputs>
        if (i.key() != OUTPUTS_DIRECTORY)
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
                for (int type = TorcOutput::Unknown; type != TorcOutput::MaxType; type++)
                {
                    if (it.key() == TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(static_cast<TorcOutput::Type>(type)))
                    {
                        QVariantMap input    = it.value().toMap();
                        QString defaultvalue = network ? input.value(QStringLiteral("default")).toString() : input.value(QStringLiteral("value")).toString();
                        QString uniqueid     = input.value(QStringLiteral("name"), QStringLiteral("Error")).toString();

                        bool ok = false;
                        double defaultdouble = defaultvalue.toDouble(&ok);
                        if (!ok)
                            defaultdouble = 0.0;

                        TorcOutput* newoutput = nullptr;
                        switch (type)
                        {
                            case TorcOutput::PWM:
                                newoutput = network ? new TorcNetworkPWMOutput(defaultdouble, input) : new TorcPWMOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::PWM), input);
                                break;
                            case TorcOutput::Switch:
                                newoutput = network ? new TorcNetworkSwitchOutput(defaultdouble, input) : new TorcSwitchOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::Switch), input);
                                break;
                            case TorcOutput::Temperature:
                                newoutput = network ? new TorcNetworkTemperatureOutput(defaultdouble, input) : new TorcTemperatureOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::Temperature), input);
                                break;
                            case TorcOutput::pH:
                                newoutput = network ? new TorcNetworkpHOutput(defaultdouble, input) : new TorcpHOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::pH), input);
                                break;
                            /*
                            case TorcOutput::Integer:
                                newoutput = network ? new TorcNetworkIntegerOutput(defaultdouble, input) : new TorcIntegerOutput(defaultdouble, DEVICE_CONSTANT + TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(TorcOutput::Integer), input);
                                break;
                            */
                            case TorcOutput::Button:
                                if (constant)
                                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot create constant button input"));
                                else
                                    newoutput = new TorcNetworkButtonOutput(defaultdouble, input);
                            default: break;
                        }

                        if (newoutput)
                            m_createdOutputs.insert(uniqueid, newoutput);
                    }
                }
            }
        }
    }
}

void TorcOutputs::Destroy(void)
{
    QWriteLocker locker(&m_handlerLock);
    QMap<QString,TorcOutput*>::const_iterator it2 = m_createdOutputs.constBegin();
    for ( ; it2 != m_createdOutputs.constEnd(); ++it2)
    {
        it2.value()->DownRef();
        TorcOutputs::gOutputs->RemoveOutput(it2.value());
    }
    m_createdOutputs.clear();
}
