/* Class TorcPiGPIO
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
#include "torcsensor.h"
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

    QVariantMap::const_iterator i = GPIO.constBegin();
    for ( ; i != GPIO.constEnd(); i++)
    {
        // GPIO can be under <sensors> or <outputs>
        if (i.key() != SENSORS_DIRECTORY && i.key() != OUTPUTS_DIRECTORY)
            continue;

        bool output = i.key() == OUTPUTS_DIRECTORY;

        QVariantMap gpio = i.value().toMap();
        QVariantMap::const_iterator it = gpio.begin();
        for ( ; it != gpio.end(); ++it)
        {
            // and find gpio
            if (it.key() == PI_GPIO)
            {
                // and find pins
                QVariantMap pins = it.value().toMap();
                QVariantMap::const_iterator it2 = pins.constBegin();
                for ( ; it2 != pins.constEnd(); ++it2)
                {
                    QVariantMap pin = it2.value().toMap();

                    if (!pin.contains("number"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("GPIO device '%1' does not specify pin <number>").arg(pin.value("name").toString()));
                        continue;
                    }

                    if (!pin.contains("default"))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("GPIO device '%1' does not specify <default> value").arg(pin.value("name").toString()));
                        continue;
                    }

                    bool ok = false;
                    int number = pin.value("number").toInt(&ok);
                    if (!ok || number < 0 || number >= NUMBER_PINS)
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse valid pin from '%1'").arg(pin.value("pin").toString()));
                        continue;
                    }

                    if (m_inputs.contains(number) || m_outputs.contains(number))
                    {
                        LOG(VB_GENERAL, LOG_ERR, QString("GPIO Pin #%1 is already in use").arg(number));
                        continue;
                    }

                    if (output)
                    {
                        TorcPiOutput* out = new TorcPiOutput(number, pin);
                        m_outputs.insert(number, out);
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_INFO, "GPIO inputs not implemented - yet");
                    }
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
