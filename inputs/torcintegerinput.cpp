/* Class TorcIntegerInput
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
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
#include "torcintegerinput.h"

TorcIntegerInput::TorcIntegerInput(double Value, const QString &ModelId, const QVariantMap &Details)
  : TorcInput(TorcInput::Integer, qRound(Value), 0, UINT32_MAX, ModelId, Details)
{
}

QStringList TorcIntegerInput::GetDescription(void)
{
    return QStringList() << tr("Constant Integer");
}

TorcInput::Type TorcIntegerInput::GetType(void)
{
    return TorcInput::Integer;
}

void TorcIntegerInput::Start(void)
{
    SetValid(true);
    TorcInput::Start();
}
