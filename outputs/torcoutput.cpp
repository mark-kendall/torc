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
#include "torcoutputs.h"
#include "torcoutput.h"

#define BLACKLIST QString("")

QString TorcOutput::TypeToString(TorcOutput::Type Type)
{
    switch (Type)
    {
        case TorcOutput::PWM:         return QString("pwm");
        case TorcOutput::Switch:      return QString("switch");
        default: break;
    }

    return QString("unknown");
}

TorcOutput::Type TorcOutput::StringToType(const QString &Type)
{
    if ("pwm" == Type)         return TorcOutput::PWM;
    if ("switch" == Type)      return TorcOutput::Switch;
    return TorcOutput::Unknown;
}

TorcOutput::TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QString &UniqueId, const QVariantMap &Details)
  : TorcDevice(true, Value, Value, ModelId, UniqueId, Details),
    TorcHTTPService(this, OUTPUTS_DIRECTORY + "/" + TypeToString(Type) + "/" + UniqueId, UniqueId, TorcOutput::staticMetaObject, BLACKLIST),
    m_owner(NULL)
{
    TorcOutputs::gOutputs->AddOutput(this);
}

bool TorcOutput::HasOwner(void)
{
    QMutexLocker locker(lock);
    return m_owner != NULL;
}

void TorcOutput::SetOwner(QObject *Owner)
{
    QMutexLocker locker(lock);

    m_owner = Owner;
    DisableMethod("SetValue");
    DisableMethod("SetValid");
}

void TorcOutput::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

TorcOutput::~TorcOutput()
{
}
