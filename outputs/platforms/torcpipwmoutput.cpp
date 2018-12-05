/* Class TorcPiPWMOutput
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2016-18
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
#define PI_PWM_RESOLUTION 1024

/*! \class TorcPiPWMOutput
 *  \brief A device to output PWM signals on the Raspberry Pi
 *
 * The Raspberry Pi supports 1 hardware PWM output on pin 1 (BCM_GPIPO 18).
 * For other pins, we use wiringPi's software PWM support and warn about its
 * usage (software PWM may utilise relatively large amounts of CPU and may be
 * subject to jitter under load - and nobody wants flickering LEDs!).
 */
TorcPiPWMOutput::TorcPiPWMOutput(int Pin, const QVariantMap &Details)
  : TorcPWMOutput(DEFAULT_VALUE, QStringLiteral("PiGPIOPWMOutput"), Details, PI_PWM_RESOLUTION),
    m_pin(Pin)
{
    // hardware PWM only operates at default 10bit accuracy
    if ((m_resolution != m_maxResolution) && (m_pin == TORC_HWPWM_PIN))
    {
        m_resolution = m_maxResolution;
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Ignoring user defined resolution for hardware PWM - defaulting to %1").arg(m_maxResolution));
    }

    int value = (int)((DEFAULT_VALUE * (double)m_resolution) + 0.5);

    if (m_pin == TORC_HWPWM_PIN)
    {
        pinMode(m_pin, PWM_OUTPUT);
        pwmWrite(m_pin, value);
    }
    else
    {
        if (softPwmCreate(m_pin, value, m_resolution) == 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Using software PWM on pin %1: It MIGHT flicker...").arg(m_pin));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to setup software PWM on pin %1").arg(m_pin));
        }
    }
}

TorcPiPWMOutput::~TorcPiPWMOutput()
{
    // always return the pin to its default state.
    // we must do this here to trigger the correct SetValue implementation
    SetValue(defaultValue);

    // shutdown the soft pwm thread
    if (m_pin != TORC_HWPWM_PIN)
        softPwmStop(m_pin);
}


QStringList TorcPiPWMOutput::GetDescription(void)
{
    return QStringList() << tr("Pin %1 PWM").arg(m_pin) << tr("Resolution %1").arg(m_resolution);
}

void TorcPiPWMOutput::SetValue(double Value)
{
    QMutexLocker locker(&lock);

    double newdouble = Value;
    if (!ValueIsDifferent(newdouble))
        return;

    int newvalue = (int)((newdouble * (double)m_resolution) + 0.5);

    if (m_pin == TORC_HWPWM_PIN)
        pwmWrite(m_pin, newvalue);
    else
        softPwmWrite(m_pin, newvalue);

    TorcPWMOutput::SetValue(Value);
}

