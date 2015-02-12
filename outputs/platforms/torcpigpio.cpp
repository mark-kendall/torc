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

// wiringPi
#include "wiringPi.h"

TorcPiGPIO* TorcPiGPIO::gPiGPIO = new TorcPiGPIO();

TorcPiGPIO::TorcPiGPIO()
  : m_lock(new QMutex(QMutex::Recursive)),
    m_setup(false)
{
    if (wiringPiSetup() > -1)
        m_setup = true;
}

TorcPiGPIO::~TorcPiGPIO()
{
    delete m_lock;
}

void TorcPiGPIO::Check(void)
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

        LOG(VB_GENERAL, LOG_INFO, QString("%1 GPIO pins available").arg(NUMBER_PINS));
    }        
}

bool TorcPiGPIO::ReservePin(int Pin, void *Owner, TorcPiGPIO::State InOut)
{
    QMutexLocker locker(m_lock);
        
    if (Pin < 0 || Pin >= NUMBER_PINS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GPIO #1 invalid").arg(Pin));
        return false;
    }

    if (!Owner)
    {
        LOG(VB_GENERAL, LOG_ERR, "Non-existent owner cannot reserve pin");
        return false;
    }

    if (InOut == TorcPiGPIO::Unused)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot reserve GPIO in Unused state");
        return false;
    }

    if (pins.contains(Pin))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GPIO #%1 already reserved").arg(Pin));
        return false;
    }

    pins.insert(Pin, QPair<TorcPiGPIO::State,void*>(InOut, Owner));
    return true;
}

void TorcPiGPIO::ReleasePin(int Pin, void *Owner)
{
    QMutexLocker locker(m_lock);

    if (Pin < 0 || Pin >= NUMBER_PINS)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GPIO #1 invalid").arg(Pin));
        return;
    }

    if (!Owner)
    {
        LOG(VB_GENERAL, LOG_ERR, "Non-existent owner cannot release pin");
        return;
    }

    if (!pins.contains(Pin))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GPIO #%1 cannot be released - not reserved").arg(Pin));
        return;
    }

    if (pins.value(Pin).second != Owner)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot release GPIO #%1 - wrong owner").arg(Pin));
        return;
    }

    pins.remove(Pin);
}
