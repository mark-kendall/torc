/* Class TorcPWMInput
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
#include "torcpwminput.h"

TorcPWMInput::TorcPWMInput(double Value, const QString &ModelId, const QVariantMap &Details)
  : TorcInput(TorcInput::PWM, Value, 0.0, 1.0, ModelId, Details)
{
}

QStringList TorcPWMInput::GetDescription(void)
{
    return QStringList() << tr("Constant PWM");
}

TorcInput::Type TorcPWMInput::GetType(void)
{
    return TorcInput::PWM;
}

void TorcPWMInput::Start(void)
{
    SetValid(true);
    TorcInput::Start();
}

double TorcPWMInput::ScaleValue(double Value)
{
    return Value;
}
