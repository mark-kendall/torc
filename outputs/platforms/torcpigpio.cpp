/* Class TorcPWMOutput
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
#include "torcoutputs.h"

// wiringPi
#include "wiringPi.h"

TorcPiGPIO* TorcPiGPIO::gPiGPIO = new TorcPiGPIO();

TorcPiGPIO::TorcPiGPIO()
  : m_setup(false)
{
    if (wiringPiSetup() > -1)
        m_setup = true;
}

TorcPiGPIO::~TorcPiGPIO()
{
}

void TorcPiGPIO::Create(const QVariantMap &GPIO)
{
    QMutexLocker locker(m_lock);

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;

        if (!m_setup)
        {
            // NB wiringPi will have terminated the program already!
            LOG(VB_GENERAL, LOG_ERR, "wiringPi is not initialised");
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QString("%1 GPIO pins available").arg(NUMBER_PINS));
        }
    }

    if (GPIO.contains("GPIO"))
    {
        QVariantMap gpio = GPIO.value("GPIO").toMap();
        QVariantMap::const_iterator it = gpio.begin();
        for ( ; it != gpio.end(); ++it)
        {
            bool ok = false;
            int pin = it.key().toInt(&ok);
            if (ok && pin >= 0 and pin < NUMBER_PINS)
            {
                QVariantMap details = it.value().toMap();

                if (!details.contains("state"))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("GPIO Pin #%1 has no state - specify input or output").arg(pin));
                    continue;
                }

                if (m_inputs.contains(pin) || m_outputs.contains(pin))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("GPIO Pin #%1 is already in use").arg(pin));
                    continue;
                }

                QString uniqueid = QString("GPIO_%1").arg(Pin);
                if (!TorcDevice::UniqueIdAvailable(uniqueid))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Device name %1 already in use - ignoring").arg(uniqueid));
                    continue;
                }

                QString name  = details.value("userName").toString();
                QString desc  = details.value("userDescription").toString();
                QString state = details.value("state").toString();

                if (state.toUpper() == "OUTPUT")
                {
                    TorcPiOutput* output = new TorcPiOutput(pin);
                    m_outputs.insert(pin, output);
                    output->SetUserName(name);
                    output->SetUserDescription(desc);
                }
                else if (state.toUpper() == "INPUT")
                {
                    LOG(VB_GENERAL, LOG_INFO, "GPIO inputs not implemented - yet");
                }
                else
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("GPIO Pin #%1 unknown state '%2'").arg(pin).arg(state));
                    continue;
                }
            }
        }
    }
}

void TorcPiGPIO::Destroy(void)
{
    QMutexLocker locker(m_lock);

/*(    QMap<int,TorcPiInput*>::iterator it = m_inputs.begin();
    for ( ; it != m_inputs.end(); ++it)
         it.value().DownRef();
    m_inputs.clear
*/
    QMap<int,TorcPiOutput*>::iterator it2 = m_outputs.begin();
    for ( ; it2 != m_outputs.end(); ++it2)
    {
         TorcOutputs::gOutputs->RemoveOutput(it2.value());
         it2.value()->DownRef();
    }
    m_outputs.clear();
}
