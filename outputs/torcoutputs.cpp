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
#include "torcoutputs.h"

#define BLACKLIST QString("")

TorcOutputs* TorcOutputs::gOutputs = new TorcOutputs();

TorcOutputs::TorcOutputs()
  : QObject(),
    TorcHTTPService(this, OUTPUTS_DIRECTORY, "outputs", TorcOutputs::staticMetaObject, BLACKLIST),
    outputList(),
    outputTypes()
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
         QString("    subgraph cluster_2 {\r\n"
                 "        label = \"%1\";\r\n"
                 "        style=filled;\r\n"
                 "        fillcolor=\"grey95\";\r\n").arg(tr("Outputs"));
    Data->append(start);

    foreach(TorcOutput* output, outputList)
    {
        QString id    = output->GetUniqueId();
        QString label = output->GetUserName();
        QString url   = output->GetPresentationURL();
        QString desc;
        QStringList source = output->GetDescription();
        foreach (QString item, source)
            if (!item.isEmpty())
                desc.append(QString(DEVICE_LINE_ITEM).arg(item));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Default %1").arg(output->GetDefaultValue())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Valid %1").arg(output->GetValid())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Value %1").arg(output->GetValue())));

        if (label.isEmpty())
            label = id;
        QString link = url.isEmpty() ? QString() : QString(" href=\"%1\"").arg(url);
        Data->append(QString("        \"%1\" [shape=record id=\"%1\" label=<<B>%2</B>%3>%4];\r\n")
            .arg(id).arg(label).arg(desc).arg(link));
    }

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
    QStringList result;
        for (int i = 0; i < TorcOutput::MaxType; ++i)
            result << TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(static_cast<TorcOutput::Type>(i));
    return result;
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
        LOG(VB_GENERAL, LOG_WARNING, QString("Already have output named %1 - ignoring").arg(Output->GetUniqueId()));
        return;
    }

    Output->UpRef();
    outputList.append(Output);
    LOG(VB_GENERAL, LOG_INFO, QString("Registered output %1").arg(Output->GetUniqueId()));
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
        LOG(VB_GENERAL, LOG_WARNING, QString("Output %1 not recognised - cannot remove").arg(Output->GetUniqueId()));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Output %1 deregistered").arg(Output->GetUniqueId()));
    Output->DownRef();
    outputList.removeOne(Output);
    emit OutputsChanged();
}
