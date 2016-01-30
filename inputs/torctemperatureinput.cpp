/* Class TorcInput
*
* Copyright (C) Mark Kendall 2014-16
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
#include "torctemperatureinput.h"

QString TorcTemperatureInput::UnitsToShortString(TorcTemperatureInput::Units Units)
{
    switch (Units)
    {
        case TorcTemperatureInput::Celsius:    return tr("°C");
        case TorcTemperatureInput::Fahrenheit: return tr("°F");
    }

    return tr("Unknown");
}

QString TorcTemperatureInput::UnitsToLongString(TorcTemperatureInput::Units Units)
{
    switch (Units)
    {
        case TorcTemperatureInput::Celsius:    return tr("Degrees Celsius");
        case TorcTemperatureInput::Fahrenheit: return tr("Degrees Fahrenheit");
    }

    return tr("Unknown");
}

TorcTemperatureInput::TorcTemperatureInput(TorcTemperatureInput::Units Units,
                                             double Value, double RangeMinimum, double RangeMaximum,
                                             const QString &ModelId, const QVariantMap &Details)
  : TorcInput(TorcInput::Temperature, Value, RangeMinimum, RangeMaximum,
              UnitsToShortString(Units), UnitsToLongString(Units), ModelId, Details),
    defaultUnits(Units),
    currentUnits(Units)
{
}

TorcTemperatureInput::~TorcTemperatureInput()
{
}

TorcInput::Type TorcTemperatureInput::GetType(void)
{
    return TorcInput::Temperature;
}

/*! \brief Return the given Value scaled for the current units.
 */
double TorcTemperatureInput::ScaleValue(double Value)
{
    QMutexLocker locker(lock);

    // no change required
    if (currentUnits == defaultUnits)
        return Value;

    // convert Celsius to Fahrenheit
    if (TorcTemperatureInput::Celsius == defaultUnits)
        return (((Value * 9.0) / 5.0) + 32.0);

    // Fahrenheit to Celsius
    return (((Value - 32.0) * 5.0) / 9.0);
}

