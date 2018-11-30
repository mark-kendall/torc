/* Class TorcSwitchInput
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
#include "torcswitchinput.h"

TorcSwitchInput::TorcSwitchInput(double Default, const QString &ModelId, const QVariantMap &Details)
  : TorcInput(TorcInput::Switch, Default, 0, 1, ModelId, Details)
{
}

QStringList TorcSwitchInput::GetDescription(void)
{
    return QStringList() << tr("Constant Switch");
}

TorcInput::Type TorcSwitchInput::GetType(void)
{
    return TorcInput::Switch;
}

void TorcSwitchInput::Start(void)
{
    SetValid(true);
    TorcInput::Start();
}

double TorcSwitchInput::ScaleValue(double Value)
{
    if (qFuzzyCompare(Value + 1.0, 1.0))
        return 0;
    return 1;
}
