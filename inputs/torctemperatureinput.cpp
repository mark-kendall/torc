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

// Torc
#include "torctemperatureinput.h"

TorcTemperatureInput::TorcTemperatureInput(double Value, double RangeMinimum, double RangeMaximum,
                                           const QString &ModelId, const QVariantMap &Details)
  : TorcInput(TorcInput::Temperature, Value, RangeMinimum, RangeMaximum, ModelId, Details)
{
}

TorcTemperatureInput::~TorcTemperatureInput()
{
}

TorcInput::Type TorcTemperatureInput::GetType(void)
{
    return TorcInput::Temperature;
}

double TorcTemperatureInput::CelsiusToFahrenheit(double Value)
{
    return (Value * 1.8) + 32.0;
}

double TorcTemperatureInput::FahrenheitToCelsius(double Value)
{
    return (Value - 32.0) / 1.8;
}
