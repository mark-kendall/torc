/* Class TorcSensor
*
* Copyright (C) Mark Kendall 2014
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
#include "torcsensors.h"
#include "torcnetworksensors.h"
#include "torcsensor.h"

#define BLACKLIST QString("SetValue,SetValid")

QString TorcSensor::TypeToString(TorcSensor::Type Type)
{
    switch (Type)
    {
        case TorcSensor::Temperature: return QString("temperature");
        case TorcSensor::pH:          return QString("pH");
        case TorcSensor::Switch:      return QString("switch");
        case TorcSensor::PWM:         return QString("pwm");
        default: break;
    }

    return QString("unknown");
}

TorcSensor::Type TorcSensor::StringToType(const QString &Type)
{
    if ("temperature" == Type) return TorcSensor::Temperature;
    if ("pH" == Type)          return TorcSensor::pH;
    if ("switch" == Type)      return TorcSensor::Switch;
    if ("pwm" == Type)         return TorcSensor::PWM;
    return TorcSensor::Unknown;
}

/*! \class Torcsensor
 *  \brief An abstract class representing an external sensor.
 *
 * TorcSensor implements a generic sensor and handles high level signalling of changes to the sensor's status
 * and/or value.
 *
 * \note Both TorcSensor and TorcHTTPService can be accessed from multiple threads. Locking is essential
 *       around non-constant member variables.
*/
TorcSensor::TorcSensor(TorcSensor::Type Type, double Value, double RangeMinimum, double RangeMaximum,
                       const QString &ShortUnits,    const QString &LongUnits,
                       const QString &ModelId,       const QString &UniqueId)
  : TorcDevice(false, Value, Value, ModelId, UniqueId),
    TorcHTTPService(this, SENSORS_DIRECTORY + "/" + TypeToString(Type) + "/" + UniqueId,
                    UniqueId, TorcSensor::staticMetaObject,
                    UniqueId.contains(NETWORK_SENSORS_STRING) ? QString("") : BLACKLIST),
    valueScaled(Value),
    operatingRangeMin(RangeMinimum),
    operatingRangeMax(RangeMaximum),
    operatingRangeMinScaled(RangeMinimum),
    operatingRangeMaxScaled(RangeMaximum),
    outOfRangeLow(true),
    outOfRangeHigh(false),
    shortUnits(ShortUnits),
    longUnits(LongUnits)
{
    // guard against stupidity
    if (operatingRangeMax <= operatingRangeMin)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Sensor has invalid operating ranges - adjusting");
        operatingRangeMax = operatingRangeMaxScaled = operatingRangeMin + 1.0;
    }

    // register this sensor - the owner must DownRef AND call RemoveSensor to ensure it is deleted.
    // N.B. gSensors is a static class
    TorcSensors::gSensors->AddSensor(this);
}

TorcSensor::~TorcSensor()
{
}

void TorcSensor::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Update the validity of the sensor.
 *
 * A sensor may not provide a meaningful value on startup or may become unavailable due to an error.
*/
void TorcSensor::SetValid(bool Valid)
{
    QMutexLocker locker(lock);

    if (Valid == valid)
        return;

    valid = Valid;
    emit ValidChanged(valid);
}

/*! \brief Update the sensors value.
 *
 * This will update both value and valueScaled and emit change signals iff necessary.
*/
void TorcSensor::SetValue(double Value)
{
    QMutexLocker locker(lock);

    // a call to SetValue implies there is a valid reading
    if (!valid)
    {
        SetValid(true);
    }
    else
    {
        // ignore status quo if already valid
        if (qFuzzyCompare(Value + 1.0f, value + 1.0f))
            return;
    }

    // update value and valueScaled
    value = Value;
    valueScaled = ScaleValue(value);
    emit ValueChanged(value);
    emit ValueScaledChanged(valueScaled);

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

/// userName is the identity given to this sensor by the end user e.g. Room, Tank
void TorcSensor::SetUserName(const QString &Name)
{
    QMutexLocker locker(lock);

    if (Name == userName)
        return;

    userName = Name;
    emit UserNameChanged(userName);
}

/// shortUnits are the abbreviated, translated units shared with the user e.g. °C.
void TorcSensor::SetShortUnits(const QString &Units)
{
    QMutexLocker locker(lock);

    if (Units == shortUnits)
        return;

    shortUnits = Units;
    emit ShortUnitsChanged(shortUnits);
}

/// longUnits are the full units shared with the user e.g. Degrees Celsisus.
void TorcSensor::SetLongUnits(const QString &Units)
{
    QMutexLocker locker(lock);

    if (Units == longUnits)
        return;

    longUnits = Units;
    emit LongUnitsChanged(longUnits);
}

/// userDescription gives a short description of the purpose of the sensor (e.g. monitor tank temperature).
void TorcSensor::SetUserDescription(const QString &Description)
{
    QMutexLocker locker(lock);

    if (Description == userDescription)
        return;

    userDescription = Description;
    emit UserDescriptionChanged(userDescription);
}

bool TorcSensor::GetValid(void)
{
    QMutexLocker locker(lock);

    return valid;
}

double TorcSensor::GetValue(void)
{
    QMutexLocker locker(lock);

    return value;
}

double TorcSensor::GetValueScaled(void)
{
    QMutexLocker locker(lock);

    return valueScaled;
}

double TorcSensor::GetOperatingRangeMin(void)
{
    // no need to lock
    return operatingRangeMin;
}

double TorcSensor::GetOperatingRangeMax(void)
{
    // no need to lock
    return operatingRangeMax;
}

double TorcSensor::GetOperatingRangeMinScaled(void)
{
    QMutexLocker locker(lock);

    return operatingRangeMinScaled;
}

double TorcSensor::GetOperatingRangeMaxScaled(void)
{
    QMutexLocker locker(lock);

    return operatingRangeMaxScaled;
}

bool TorcSensor::GetOutOfRangeLow(void)
{
    QMutexLocker locker(lock);

    return outOfRangeLow;
}

bool TorcSensor::GetOutOfRangeHigh(void)
{
    QMutexLocker locker(lock);

    return outOfRangeHigh;
}

QString TorcSensor::GetShortUnits(void)
{
    QMutexLocker locker(lock);

    return shortUnits;
}

QString TorcSensor::GetLongUnits(void)
{
    QMutexLocker locker(lock);

    return longUnits;
}

/// modelId gives a hint as to the type and functionality of the sensor.
QString TorcSensor::GetModelId(void)
{
    // no need to lock
    return modelId;
}

/// uniqueId should provide an unambiguous identifier for the sensor.
QString TorcSensor::GetUniqueId(void)
{
    // no need to lock
    return uniqueId;
}

QString TorcSensor::GetUserName(void)
{
    QMutexLocker locker(lock);

    return userName;
}

QString TorcSensor::GetUserDescription(void)
{
    QMutexLocker locker(lock);

    return userDescription;
}
