/* Class Torc1WireDS18B20
*
* This file is part of the Torc project.
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Torc
#include "torc1wireds18b20.h"

Torc1WireDS18B20::Torc1WireDS18B20(const QString &UniqueId)
  : TorcTemperatureSensor(TorcTemperatureSensor::Celsius, 0, -55.0, 125.0,
                          DS18B20NAME, UniqueId)
{
}

Torc1WireDS18B20::~Torc1WireDS18B20()
{
}

