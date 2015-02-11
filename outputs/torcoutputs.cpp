/* Class TorcOutput
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
#include "torcoutputs.h"

#define BLACKLIST QString("")

TorcOutputs* TorcOutputs::gOutputs = new TorcOutputs();

TorcOutputs::TorcOutputs()
  : QObject(),
    TorcHTTPService(this, OUTPUTS_DIRECTORY, "Outputs", TorcOutputs::staticMetaObject, BLACKLIST),
    m_lock(new QMutex(QMutex::Recursive))
{
}

TorcOutputs::~TorcOutputs()
{
    delete m_lock;
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
    QMutexLocker locker(m_lock);

    // iterate over our list for each output type
    for (int type = TorcOutput::Unknown; type < TorcOutput::MaxType; type++)
    {
        QStringList outputsfortype;
        foreach (TorcOutput *output, outputList)
            if (output->GetType() == type)
                outputsfortype.append(TorcOutput::TypeToString(output->GetType()) + "/" + output->GetUniqueId());

        if (!outputsfortype.isEmpty())
            result.insert(TorcOutput::TypeToString(static_cast<TorcOutput::Type>(type)), outputsfortype);
    }
    return result;
}

void TorcOutputs::AddOutput(TorcOutput *Output)
{
    QMutexLocker locker(m_lock);
    if (!Output)
        return;

    if (outputList.contains(Output))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Already have output named %1 - ignoring").arg(Output->GetUniqueId()));
        return;
    }

    Output->UpRef();
    outputList.append(Output);
    LOG(VB_GENERAL, LOG_INFO, QString("Added output %1 to list").arg(Output->GetUniqueId()));
    emit OutputsChanged();
}

void TorcOutputs::RemoveOutput(TorcOutput *Output)
{
    QMutexLocker locker(m_lock);
    if (!Output)
        return;

    if (!outputList.contains(Output))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Output %1 not recognised - cannot remove").arg(Output->GetUniqueId()));
        return;
    }

    Output->DownRef();
    outputList.removeOne(Output);
    LOG(VB_GENERAL, LOG_INFO, QString("Output %1 removed from list").arg(Output->GetUniqueId()));
    emit OutputsChanged();
}
