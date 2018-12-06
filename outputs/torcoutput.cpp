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
#include "torcoutputs.h"
#include "torcoutput.h"

#define BLACKLIST QStringLiteral("SetValue,SetValid")

// N.B. We need to pass the staticMetaObject to TorcHTTPService as the object is not yet complete.
//      If we pass 'this' during construction, this->metaObject() only contains details of the super class.
TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details)
  : TorcDevice(true, Value, Value, ModelId, Details),
    TorcHTTPService(this, QStringLiteral("%1/%2%3").arg(OUTPUTS_DIRECTORY, TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(Type), Details.value(QStringLiteral("name")).toString()),
                    Details.value(QStringLiteral("name")).toString(), TorcOutput::staticMetaObject, BLACKLIST),
    m_owner(nullptr)
{
    TorcOutputs::gOutputs->AddOutput(this);
}

TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details,
                       QObject *Output, const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcDevice(true, Value, Value, ModelId, Details),
    TorcHTTPService(Output, QStringLiteral("%1/%2/%3").arg(OUTPUTS_DIRECTORY, TorcCoreUtils::EnumToLowerString<TorcOutput::Type>(Type), Details.value(QStringLiteral("name")).toString()),
                    Details.value(QStringLiteral("name")).toString(), MetaObject, BLACKLIST + "," + Blacklist),
    m_owner(nullptr)
{
    TorcOutputs::gOutputs->AddOutput(this);
}

bool TorcOutput::HasOwner(void)
{
    QMutexLocker locker(&lock);
    return m_owner != nullptr;
}

QString TorcOutput::GetUIName(void)
{
    QMutexLocker locker(&lock);
    if (userName.isEmpty())
        return uniqueId;
    return userName;
}

bool TorcOutput::SetOwner(QObject *Owner)
{
    QMutexLocker locker(&lock);

    if (!Owner)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot set NULL output owner for %1").arg(uniqueId));
        return false;
    }

    if (m_owner && m_owner != Owner)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Cannot set different output owner for %1").arg(uniqueId));
        return false;
    }

    m_owner = Owner;

    return true;
}

void TorcOutput::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

void TorcOutput::Graph(QByteArray *Data)
{
    if (!Data)
        return;

    QString id    = GetUniqueId();
    QString label = GetUserName();
    QString url   = GetPresentationURL();
    QString desc;
    QStringList source = GetDescription();
    foreach (const QString &item, source)
        if (!item.isEmpty())
            desc.append(QString(DEVICE_LINE_ITEM).arg(item));
    desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Default %1").arg(GetDefaultValue())));
    desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Valid %1").arg(GetValid())));
    desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Value %1").arg(GetValue())));

    if (label.isEmpty())
        label = id;
    QString link = url.isEmpty() ? QString() : QStringLiteral(" href=\"%1\"").arg(url);
    Data->append(QStringLiteral("        \"%1\" [shape=record id=\"%1\" label=<<B>%2</B>%3>%4];\r\n").arg(id, label, desc, link));
}
