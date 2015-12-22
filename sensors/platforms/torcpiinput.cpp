/* Class TorcPiInput
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
#include "torclogging.h"
#include "torcpigpio.h"
#include "torcpiinput.h"

// wiringPi
#include "wiringPi.h"

#define DEFAULT_VALUE 0

TorcPiInput::TorcPiInput(int Pin, const QVariantMap &Details)
  : TorcSwitchSensor(DEFAULT_VALUE, "PiGPIOInput", Details),
    m_pin(Pin)
{
    // setup pin for input
    pinMode(m_pin, INPUT);

    // for now, just assume pull up/down resistors have been added externally and disable
    // the internals
    pullUpDnControl(m_pin, PUD_OFF);
}

TorcPiInput::~TorcPiInput()
{
}

void TorcPiInput::Start(void)
{
    // start listening for interrupts here...

    // and call the default implementation
    TorcSwitchSensor::Start();
}

QStringList TorcPiInput::GetDescription(void)
{
    return QStringList() << tr("Pin %1").arg(m_pin);
}


