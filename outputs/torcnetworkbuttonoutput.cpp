/* Class TorcNetworkButtonOutput
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
#include "torcnetworkbuttonoutput.h"

/*! \class TorcNetworkButtonOutput
 *  \brief A network output that mimics the behaviour of a mechanical push button.
 *
 * A traditional push button will toggle from low to high to low (or high to low to high)
 * as it is pressed and released. This behaviour is mimicked.
 *
 * Most useful in shadowing a 'real' pushbutton to give a network equivalent for dual control or
 * for triggering other actions - usually in conjunction with the 'toggle' type control.
 *
 * \note SetValue will trigger the push button action irrespective of the current value. Be
 *       sure to set default value to the correct default state.
 * \note Linking this device to a network button input is unlikely to generate useful behaviour.
 * \sa TorcNetworkButtonSensor
*/
TorcNetworkButtonOutput::TorcNetworkButtonOutput(double Default, const QVariantMap &Details)
  : TorcNetworkSwitchOutput(Default, Details)
{
    // timers cannot be started from other threads, so use some signalling
    connect(this, SIGNAL(Pushed()), this, SLOT(StartTimer()));

    // setup the timer
    m_pulseTimer.setSingleShot(true);
    m_pulseTimer.setInterval(5);
    connect(&m_pulseTimer, SIGNAL(timeout()), this, SLOT(EndPulse()));
}

TorcNetworkButtonOutput::~TorcNetworkButtonOutput()
{
}

QStringList TorcNetworkButtonOutput::GetDescription(void)
{
    return QStringList() << tr("Network button");
}

/*! \brief Toggle the value of the button.
 *
 * The input value is ignored, the current value is toggled and the timer started to
 * revert the value to its original state after a small period of time. This mimics the actions
 * of a real, mechanical push button for consistency with those devices.
*/
void TorcNetworkButtonOutput::SetValue(double Value)
{
    (void)Value;
    QMutexLocker locker(lock);

    if (m_pulseTimer.isActive())
        return;

    TorcSwitchOutput::SetValue(value < 1.0 ? 1.0 : 0.0);
    emit Pushed();
}

void TorcNetworkButtonOutput::StartTimer(void)
{
    if (!m_pulseTimer.isActive())
        m_pulseTimer.start();
}

void TorcNetworkButtonOutput::EndPulse(void)
{
    QMutexLocker locker(lock);

    TorcSwitchOutput::SetValue(value < 1.0 ? 1.0 : 0.0);
}


