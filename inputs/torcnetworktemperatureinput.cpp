/* Class TorcNetworkTemperatureInput
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
#include "torcnetworktemperatureinput.h"

// TODO these defaults may not be the best
TorcNetworkTemperatureInput::TorcNetworkTemperatureInput(double Default, const QVariantMap &Details)
  : TorcTemperatureInput(Default, -1000, 1000, "NetworkTemperatureInput", Details)
{
}

QStringList TorcNetworkTemperatureInput::GetDescription(void)
{
    return QStringList() << tr("Network temperature");
}

void TorcNetworkTemperatureInput::Start(void)
{
    SetValid(true);
    SetValue(defaultValue);
    // NB skip TorcTemperatureInput::Start
    TorcInput::Start();
}

