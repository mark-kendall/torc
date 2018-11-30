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

// Qt
#include <QMetaEnum>

// Torc
#include "torclogging.h"
#include "torcoutputs.h"
#include "torcoutput.h"

#define BLACKLIST QString("SetValue,SetValid")

QString TorcOutput::TypeToString(TorcOutput::Type Type)
{
    return QString(QMetaEnum::fromType<TorcOutput::Type>().valueToKey(Type)).toLower();
}

int TorcOutput::StringToType(const QString &Type, bool CaseSensitive)
{
    const QMetaEnum metaEnum = QMetaEnum::fromType<TorcOutput::Type>();
    if (CaseSensitive)
        return metaEnum.keyToValue(Type.toLatin1());

    QByteArray type = Type.toLower().toLatin1();
    int count = metaEnum.keyCount();
    for (int i = 0; i < count; i++)
        if (qstrcmp(type, QByteArray(metaEnum.key(count)).toLower()) == 0)
            return metaEnum.value(count);
    return -1; // for consistency with QMetaEnum::keyToValue
}

// N.B. We need to pass the staticMetaObject to TorcHTTPService as the object is not yet complete.
//      If we pass 'this' during construction, this->metaObject() only contains details of the super class.
TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details)
  : TorcDevice(true, Value, Value, ModelId, Details),
    TorcHTTPService(this, OUTPUTS_DIRECTORY + "/" + TypeToString(Type) + "/" + Details.value("name").toString(),
                    Details.value("name").toString(), TorcOutput::staticMetaObject, BLACKLIST),
    m_owner(nullptr)
{
    TorcOutputs::gOutputs->AddOutput(this);
}

TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details,
                       QObject *Output, const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcDevice(true, Value, Value, ModelId, Details),
    TorcHTTPService(Output, OUTPUTS_DIRECTORY + "/" + TypeToString(Type) + "/" + Details.value("name").toString(),
                    Details.value("name").toString(), MetaObject, BLACKLIST + "," + Blacklist),
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
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot set NULL output owner for %1").arg(uniqueId));
        return false;
    }

    if (m_owner && m_owner != Owner)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot set different output owner for %1").arg(uniqueId));
        return false;
    }

    m_owner = Owner;

    return true;
}

void TorcOutput::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}
