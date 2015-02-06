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

// Torc
#include "torctemperaturesensor.h"

QString TorcTemperatureSensor::UnitsToShortString(TorcTemperatureSensor::Units Units)
{
    switch (Units)
    {
        case TorcTemperatureSensor::Celsius: return tr("°C");
        case TorcTemperatureSensor::Fahrenheit: return tr("°F");
    }

    return tr("Unknown");
}

QString TorcTemperatureSensor::UnitsToLongString(TorcTemperatureSensor::Units Units)
{
    switch (Units)
    {
        case TorcTemperatureSensor::Celsius: return tr("Degrees Celsius");
        case TorcTemperatureSensor::Fahrenheit: return tr("Degrees Fahrenheit");
    }

    return tr("Unknown");
}

TorcTemperatureSensor::TorcTemperatureSensor(TorcTemperatureSensor::Units Units,
                                             double Value, double RangeMinimum, double RangeMaximum,
                                             const QString &ModelId, const QString &UniqueId)
  : TorcSensor(TorcSensor::Temperature, Value, RangeMinimum, RangeMaximum,
               UnitsToShortString(Units), UnitsToLongString(Units), ModelId, UniqueId),
    defaultUnits(Units),
    currentUnits(Units)
{
}

TorcTemperatureSensor::~TorcTemperatureSensor()
{
}

TorcSensor::Type TorcTemperatureSensor::GetType(void)
{
    return TorcSensor::Temperature;
}

/*! \brief Return the given Value scaled for the current units.
 */
double TorcTemperatureSensor::ScaleValue(double Value)
{
    QMutexLocker locker(m_lock);

    // no change required
    if (currentUnits == defaultUnits)
        return Value;

    // convert Celsius to Fahrenheit
    if (TorcTemperatureSensor::Celsius == defaultUnits)
        return (((Value * 9.0) / 5.0) + 32.0);

    // Fahrenheit to Celsius
    return (((Value - 32.0) * 5.0) / 9.0);
}

