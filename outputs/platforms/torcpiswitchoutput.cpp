/* Class TorcPiSwitchOutput
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
#include "torclogging.h"
#include "torcpigpio.h"
#include "torcpiswitchoutput.h"

// wiringPi
#include "wiringPi.h"

#define DEFAULT_VALUE 0

TorcPiSwitchOutput::TorcPiSwitchOutput(int Pin, const QVariantMap &Details)
  : TorcSwitchOutput(DEFAULT_VALUE, QStringLiteral("PiGPIOSwitchOutput"), Details),
    m_pin(Pin)
{
    // setup and initialise pin for output
    pinMode(m_pin, OUTPUT);
    digitalWrite(m_pin, DEFAULT_VALUE);
}

TorcPiSwitchOutput::~TorcPiSwitchOutput()
{
    // always return the pin to its default state.
    // we must do this here to trigger the correct SetValue implementation
    SetValue(defaultValue);
}

QStringList TorcPiSwitchOutput::GetDescription(void)
{
    return QStringList() << tr("Pin %1 Switch").arg(m_pin);
}

void TorcPiSwitchOutput::SetValue(double Value)
{
    QMutexLocker locker(&lock);

    // as in TorcSwitchOutput::SetValue
    double newvalue = Value == 0.0 ? 0 : 1;

    // ignore same value updates
    if (qFuzzyCompare(newvalue + 1.0f, value + 1.0f))
        return;

    digitalWrite(m_pin, newvalue);
    TorcSwitchOutput::SetValue(newvalue);
}
