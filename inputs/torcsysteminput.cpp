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
 * Currently limited to listening for Start (and Stop) events to enable, for example,
 * a soft start for dimmed lights.
*/
TorcSystemInput::TorcSystemInput(double Value, const QVariantMap &Details)
  : TorcInput(TorcInput::SystemStarted, Value, 0, 1, "SystemStarted", Details)
{
    gLocalContext->AddObserver(this);
    SetValid(true);
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
    return QStringList() << tr("System started");
}

bool TorcSystemInput::event(QEvent *Event)
{
    if (Event && (Event->type() == TorcEvent::TorcEventType))
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            if (torcevent->GetEvent() == Torc::Start)
                SetValue(1.0);
            else if (torcevent->GetEvent() == Torc::Stop)
                SetValue(0.0);
        }
    }

    return TorcInput::event(Event);
}
