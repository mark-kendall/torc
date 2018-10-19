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
#include "torcoutputs.h"
#include "torcoutput.h"

#define BLACKLIST QString("SetValue,SetValid")

QString TorcOutput::TypeToString(TorcOutput::Type Type)
{
    switch (Type)
    {
        case TorcOutput::PWM:         return QString("pwm");
        case TorcOutput::Switch:      return QString("switch");
        case TorcOutput::Temperature: return QString("temperature");
        case TorcOutput::pH:          return QString("pH");
        case TorcOutput::Camera:      return QString("camera");
        default: break;
    }

    return QString("unknown");
}

TorcOutput::Type TorcOutput::StringToType(const QString &Type)
{
    QString type = Type.toLower();
    if ("pwm" == type)         return TorcOutput::PWM;
    if ("switch" == type)      return TorcOutput::Switch;
    if ("temperature" == type) return TorcOutput::Temperature;
    if ("ph" == type)          return TorcOutput::pH;
    if ("camera" == type)      return TorcOutput::Camera;
    return TorcOutput::Unknown;
}

TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details)
  : TorcDevice(true, Value, Value, ModelId, Details),
    TorcHTTPService(this, OUTPUTS_DIRECTORY + "/" + TypeToString(Type) + "/" + Details.value("name").toString(),
                    Details.value("name").toString(), TorcOutput::staticMetaObject, BLACKLIST),
    m_owner(NULL)
{
    if (!uniqueId.isEmpty())
        TorcOutputs::gOutputs->AddOutput(this);
}

TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details,
                       QObject *Output, const QMetaObject &MetaObject, const QString &Blacklist)
  : TorcDevice(true, Value, Value, ModelId, Details),
    TorcHTTPService(Output, OUTPUTS_DIRECTORY + "/" + TypeToString(Type) + "/" + Details.value("name").toString(),
                    Details.value("name").toString(), MetaObject, BLACKLIST + "," + Blacklist),
    m_owner(NULL)
{
    if (!uniqueId.isEmpty())
        TorcOutputs::gOutputs->AddOutput(this);
}

bool TorcOutput::HasOwner(void)
{
    QMutexLocker locker(&lock);
    return m_owner != NULL;
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

TorcOutput::~TorcOutput()
{
}
