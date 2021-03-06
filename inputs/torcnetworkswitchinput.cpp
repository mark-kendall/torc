/* Class TorcNetworkSwitchInput
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
#include "torcnetworkswitchinput.h"

TorcNetworkSwitchInput::TorcNetworkSwitchInput(double Default, const QVariantMap &Details)
  : TorcSwitchInput(Default, QStringLiteral("NetworkSwitch"), Details)
{
}

QStringList TorcNetworkSwitchInput::GetDescription(void)
{
    return QStringList() << tr("Network switch");
}

void TorcNetworkSwitchInput::Start(void)
{
    SetValid(true);
    SetValue(defaultValue);
    // NB skip TorcSwitchInput::Start
    TorcInput::Start();
}

