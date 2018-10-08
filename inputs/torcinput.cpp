/* Class TorcInput
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Qt
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torcinputs.h"
#include "torcnetworkinputs.h"
#include "torcinput.h"

#define BLACKLIST QString("SetValue,SetValid")

QString TorcInput::TypeToString(TorcInput::Type Type)
{
    switch (Type)
    {
        case TorcInput::Temperature:   return QString("temperature");
        case TorcInput::pH:            return QString("pH");
        case TorcInput::Switch:        return QString("switch");
        case TorcInput::PWM:           return QString("pwm");
        case TorcInput::Button:        return QString("button");
        case TorcInput::SystemStarted: return QString("started");
        default: break;
    }

    return QString("unknown");
}

TorcInput::Type TorcInput::StringToType(const QString &Type)
{
    QString type = Type.toLower();
    if ("temperature" == type) return TorcInput::Temperature;
    if ("ph"          == type) return TorcInput::pH;
    if ("switch"      == type) return TorcInput::Switch;
    if ("pwm"         == type) return TorcInput::PWM;
    if ("started"     == type) return TorcInput::SystemStarted;
    return TorcInput::Unknown;
}

/*! \class Torcinput
 *  \brief An abstract class representing an external input.
 *
 * TorcInput implements a generic input and handles high level signalling of changes to the input's status
 * and/or value.
 *
 * \note Both TorcInput and TorcHTTPService can be accessed from multiple threads. Locking is essential
 *       around non-constant member variables.
*/
TorcInput::TorcInput(TorcInput::Type Type, double Value, double RangeMinimum, double RangeMaximum,
                     const QString &ModelId, const QVariantMap &Details)
  : TorcDevice(false, Value, Value, ModelId, Details),
    TorcHTTPService(this, SENSORS_DIRECTORY + "/" + TypeToString(Type) + "/" + Details.value("name").toString(),
                    Details.value("name").toString(), TorcInput::staticMetaObject,
                    ModelId.startsWith("Network") ? QString("") : BLACKLIST),
    operatingRangeMin(RangeMinimum),
    operatingRangeMax(RangeMaximum),
    outOfRangeLow(true),
    outOfRangeHigh(false)
{
    // guard against stupidity
    if (operatingRangeMax <= operatingRangeMin)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Input has invalid operating ranges - adjusting");
        operatingRangeMax = operatingRangeMin + 1.0;
    }

    // register this input - the owner must DownRef AND call RemoveInput to ensure it is deleted.
    // N.B. gInputs is a static class
    if (!uniqueId.isEmpty())
        TorcInputs::gInputs->AddInput(this);
}

TorcInput::~TorcInput()
{
}

void TorcInput::Start(void)
{
    // emit value first, as this will indicate a valid value.
    // then emit valid to force its state.
    emit ValueChanged(value);
    emit ValidChanged(valid);
}

QString TorcInput::GetUIName(void)
{
    QMutexLocker locker(&lock);
    if (userName.isEmpty())
        return uniqueId;
    return userName;
}

void TorcInput::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Update the inputs value.
 *
 * This will update both value and valueScaled and emit change signals iff necessary.
*/
void TorcInput::SetValue(double Value)
{
    QMutexLocker locker(&lock);

    // a call to SetValue implies there is a valid reading
    if (!valid)
    {
        SetValid(true);
    }
    else
    {
        // ignore status quo if already valid
        if (wasInvalid)
        {
            wasInvalid = false;
        }
        else
        {
            if (qFuzzyCompare(Value + 1.0f, value + 1.0f))
                return;
        }
    }

    // update value and valueScaled
    value = Value;
    emit ValueChanged(value);

    // check for out of operating range - this works with default/unscaled values
    // N.B. use >= to allow for binary operation (e.g. a switch is either 0 or 1)
    if (value >= operatingRangeMax)
    {
        // signal changes - cannot be both states at once
        if (outOfRangeLow)
            emit OutOfRangeLowChanged(false);
        outOfRangeLow = false;

        if (!outOfRangeHigh)
            emit OutOfRangeHighChanged(true);
        outOfRangeHigh = true;
    }
    else if (value <= operatingRangeMin)
    {
        // as above
        if (outOfRangeHigh)
            emit OutOfRangeHighChanged(false);
        outOfRangeHigh = false;

        if (!outOfRangeLow)
            emit OutOfRangeLowChanged(true);
        outOfRangeLow  = true;
    }
}

void TorcInput::SetValid(bool Valid)
{
    QMutexLocker locker(&lock);

    if (!Valid)
        SetValue(defaultValue);

    TorcDevice::SetValid(Valid);
}

double TorcInput::GetOperatingRangeMin(void)
{
    // no need to lock
    return operatingRangeMin;
}

double TorcInput::GetOperatingRangeMax(void)
{
    // no need to lock
    return operatingRangeMax;
}

bool TorcInput::GetOutOfRangeLow(void)
{
    QMutexLocker locker(&lock);

    return outOfRangeLow;
}

bool TorcInput::GetOutOfRangeHigh(void)
{
    QMutexLocker locker(&lock);

    return outOfRangeHigh;
}
