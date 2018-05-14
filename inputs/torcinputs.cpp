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
#include "torcadminthread.h"
#include "torccentral.h"
#include "torcinputs.h"

#define BLACKLIST QString("")

TorcInputs* TorcInputs::gInputs = new TorcInputs();

/*! \brief TorcInputs contains the master record of known inputs.
 *
 * A static global TorcInputs object is created (this could alternatively be created
 * as a TorcAdminObject). This object will list these inputs as an HTTP service.
 *
 * Any subclass of TorcInput will automatically register itself on creation. Any class
 * creating inputs must DownRef AND call RemoveInput when deleting them.
 *
 * \todo Translations and constants for web client.
 * \note This class is thread safe.
*/
TorcInputs::TorcInputs()
  : QObject(),
    TorcHTTPService(this, SENSORS_DIRECTORY, "inputs", TorcInputs::staticMetaObject, BLACKLIST),
    inputList(),
    inputTypes(),
    m_lock(QMutex::Recursive)
{
}

TorcInputs::~TorcInputs()
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
          QString("    subgraph cluster_0 {\r\n"
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
        foreach (QString item, source)
            desc.append(QString(DEVICE_LINE_ITEM).arg(item));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Default %1").arg(input->GetDefaultValue())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Valid %1").arg(input->GetValid())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Value %1").arg(input->GetValue())));

        if (label.isEmpty())
            label = id;
        Data->append(QString("        \"%1\" [shape=record id=\"%1\" label=<<B>%2</B>%3>];\r\n")
            .arg(id).arg(label).arg(desc));
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
    QMutexLocker locker(&m_lock);

    // iterate over our list for each input type
    for (int type = TorcInput::Unknown; type < TorcInput::MaxType; type++)
    {
        QStringList inputsfortype;
        foreach (TorcInput *input, inputList)
            if (input->GetType() == type)
                inputsfortype.append(input->GetUniqueId());

        if (!inputsfortype.isEmpty())
            result.insert(TorcInput::TypeToString(static_cast<TorcInput::Type>(type)), inputsfortype);
    }
    return result;
}

QStringList TorcInputs::GetInputTypes(void)
{
    QStringList result;
        for (int i = 0; i < TorcInput::MaxType; ++i)
            result << TorcInput::TypeToString((TorcInput::Type)i);
    return result;
}

void TorcInputs::AddInput(TorcInput *Input)
{
    QMutexLocker locker(&m_lock);
    if (!Input)
        return;

    if (inputList.contains(Input))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Already have an input named %1 - ignoring").arg(Input->GetUniqueId()));
        return;
    }

    Input->UpRef();
    inputList.append(Input);
    LOG(VB_GENERAL, LOG_INFO, QString("Registered input '%1'").arg(Input->GetUniqueId()));
    emit InputsChanged();
}

void TorcInputs::RemoveInput(TorcInput *Input)
{
    QMutexLocker locker(&m_lock);
    if (!Input)
        return;

    if (!inputList.contains(Input))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Input %1 not recognised - cannot remove").arg(Input->GetUniqueId()));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Input %1 de-registered").arg(Input->GetUniqueId()));
    Input->DownRef();
    inputList.removeOne(Input);
    emit InputsChanged();
}
