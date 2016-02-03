/* Class TorcPiPWMOutput
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2016
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
#include "torcpipwmoutput.h"

// wiringPi
#include "wiringPi.h"
#include "softPwm.h"

#define DEFAULT_VALUE 0

/*! \class TorcPiPWMOutput
 *  \brief A device to output PWM signals on the Raspberry Pi
 *
 * The Raspberry Pi supports 1 hardware PWM output on pin 1 (BCM_GPIPO 18).
 * For other pins, we use wiringPi's software PWM support and warn about its
 * usage (software PWM may utilise relatively large amounts of CPU and may be
 * subject to jitter under load - and nobody wants flickering LEDs!).
 */
TorcPiPWMOutput::TorcPiPWMOutput(int Pin, const QVariantMap &Details)
  : TorcPWMOutput(DEFAULT_VALUE, "PiGPIOPWMOutput", Details),
    m_pin(Pin)
{
    if (m_pin == TORC_HWPWM_PIN)
    {
        pinMode(m_pin, PWM_OUTPUT);
        digitalWrite(m_pin, DEFAULT_VALUE);
    }
    else
    {
        if (softPwmCreate(m_pin, DEFAULT_VALUE, 100 == 0))
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Using software PWM on pin %1: It MIGHT flicker...").arg(m_pin));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to setup software PWM on pin %1").arg(m_pin));
        }
    }
}

TorcPiPWMOutput::~TorcPiPWMOutput()
{
    // always return the pin to its default state.
    // we must do this here to trigger the correct SetValue implementation
    SetValue(defaultValue);

    // shutdown the soft pwm thread
    if (m_pin == TORC_HWPWM_PIN)
        softPwmStop(m_pin);
}


QStringList TorcPiPWMOutput::GetDescription(void)
{
    return QStringList() << tr("Pin %1 PWM").arg(m_pin);
}

void TorcPiPWMOutput::SetValue(double Value)
{
    QMutexLocker locker(lock);

    // ignore same value updates
    if (qFuzzyCompare(Value + 1.0f, value + 1.0f))
        return;

    if (m_pin == TORC_HWPWM_PIN)
    {
        // hw pwm has a range of 1024
        int value = (int)((Value * 1024.0) + 0.5);
        pwmWrite(m_pin, value);
    }
    else
    {
        // we use a range of 100 for soft pwm
        int value = (int)((Value * 100) + 0.5);
        softPwmWrite(m_pin, value);
    }

    TorcPWMOutput::SetValue(Value);
}

