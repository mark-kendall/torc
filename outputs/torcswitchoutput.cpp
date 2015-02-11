/* Class TorcSwitchOutput
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
#include "torcswitchoutput.h"

TorcSwitchOutput::TorcSwitchOutput(double Value, const QString &ModelId, const QString &UniqueId)
  : TorcOutput(TorcOutput::Switch, Value, ModelId, UniqueId)
{
}

TorcSwitchOutput::~TorcSwitchOutput()
{
}

TorcOutput::Type TorcSwitchOutput::GetType(void)
{
    return TorcOutput::Switch;
}

void TorcSwitchOutput::SetValue(double Value)
{
    // a switch operates as 0 or 1.
    // for our purposes anything non-zero is 1

    if (qFuzzyCompare(Value + 1.0, 1.0))
    {
        TorcOutput::SetValue(0);
        return;
    }

    TorcOutput::SetValue(1);
}
