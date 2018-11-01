/* Class TorcSystemInput
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
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

#include "torclocalcontext.h"
#include "torcsysteminput.h"

/*! \class TorcSystemInput
 *
 * This device listens for Torc events in order to trigger changes in state
 * based on the current Torc status.
 * Currently limited to listening for Start and Stop events to enable, for example,
 * a soft start and finish for dimmed lights.
 *
 * An optional delay parameter will delay stopping the application by the given number of seconds to
 * allow for dimming etc.
*/
TorcSystemInput::TorcSystemInput(double Value, const QVariantMap &Details)
  : TorcInput(TorcInput::SystemStarted, Value, 0, 1, "SystemStarted", Details),
    m_shutdownDelay(0)
{
    gLocalContext->AddObserver(this);
    SetValid(true);

    if (Details.contains("delay"))
    {
        bool ok = false;
        uint delay = Details.value("delay").toInt(&ok);
        if (ok && (delay > 0))
        {
            gLocalContext->SetShutdownDelay(delay);
            m_shutdownDelay = delay;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, "Failed to parse meaningful value for delay (>0)");
        }
    }
}

TorcSystemInput::~TorcSystemInput(void)
{
    gLocalContext->RemoveObserver(this);
}

TorcInput::Type TorcSystemInput::GetType(void)
{
    return TorcInput::SystemStarted;
}

QStringList TorcSystemInput::GetDescription(void)
{
    return QStringList() << tr("System started") << tr("Shutdown delay %1").arg(m_shutdownDelay);
}

bool TorcSystemInput::event(QEvent *Event)
{
    if (Event && (Event->type() == TorcEvent::TorcEventType))
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            switch (torcevent->GetEvent())
            {
                case Torc::Stop:
                case Torc::TorcWillStop:
                    SetValue(0.0);
                    break;
                case Torc::Start:
                    SetValue(1.0);
                    break;
                default: break;
            }
        }
    }

    return TorcInput::event(Event);
}
