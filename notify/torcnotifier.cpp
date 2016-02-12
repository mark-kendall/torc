/* Class TorcNotifier
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torclogging.h"
#include "torcnotifier.h"

/*! \page notifiers Notifiers
 *
 * Notifiers send \link notifications notifications\endlink to external/third party applications.
 *
 * \class TorcNotifier
 *
 * This is the base notifier class. Subclass it and implement Notify to send messages
 * to the world.
 *
 * \note TorcNotifier classes may operate asynchronously and may listen for evetns. They thus may
 *  need to run in a full QThread (and not a QRunnable).
*/
TorcNotifier::TorcNotifier(const QVariantMap &Details)
  : TorcDevice(false, 0, 0, "Notifier", Details)
{
}

TorcNotifier::~TorcNotifier()
{
}

bool TorcNotifier::event(QEvent *Event)
{
    return TorcDevice::event(Event);
}

TorcNotifierFactory* TorcNotifierFactory::gTorcNotifierFactory = NULL;

TorcNotifierFactory::TorcNotifierFactory()
{
    nextTorcNotifierFactory = gTorcNotifierFactory;
    gTorcNotifierFactory = this;
}

TorcNotifierFactory::~TorcNotifierFactory()
{
}

TorcNotifierFactory* TorcNotifierFactory::GetTorcNotifierFactory(void)
{
    return gTorcNotifierFactory;
}

TorcNotifierFactory* TorcNotifierFactory::NextFactory(void) const
{
    return nextTorcNotifierFactory;
}

