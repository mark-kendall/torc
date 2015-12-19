/* Class TorcNetworkTemperatureSensor
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
#include "torcnetworktemperaturesensor.h"

// TODO these defaults may not be the best
TorcNetworkTemperatureSensor::TorcNetworkTemperatureSensor(double Default, const QVariantMap &Details)
  : TorcTemperatureSensor(TorcTemperatureSensor::Celsius, Default, -100, 100, "NetworkTemperatureSensor", Details)
{
}

TorcNetworkTemperatureSensor::~TorcNetworkTemperatureSensor()
{
}

QStringList TorcNetworkTemperatureSensor::GetDescription(void)
{
    return QStringList() << tr("Network temperature");
}


