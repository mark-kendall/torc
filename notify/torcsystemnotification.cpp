/* Class TorcSystemNotification
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
#include "torclocalcontext.h"
#include "torcnotify.h"
#include "torcsystemnotification.h"

/*! \class TorcSystemNotification
 *
 * TorcSystemNotification listens for Torc system events, such as Start, Stop, Suspending etc and messages
 * when the specified events are seen.
 *
 * The configuration file must specifiy a comma separated list of events (per the Torc definitions) and
 * a comma separated list of notifiers who will send the appropriate messages.
 *
 * Example configuration:-
 * \code
 * <notify>
 *   <notifications>
 *     <system>
 *       <name>systemtest</name>
 *       <inputs>
 *         <event>start</event>
 *         <event>stop</event>
 *       </inputs>
 *       <outputs>testnotifier,testnotifier2</outputs>
 *       <message>
 *         <!-- title is optional -->
 *         <title>%applicationname% message:</title>
 *         <!-- body is mandatory -->
 *         <body>System event: '%event%' (%shortdatetime%)</body>
 *       </message>
 *     </system>
 *   </notifications>
 * </notify>
 * \endcode
 *
 * \todo Validate the list of events against those specified in Torc::Actions.
*/
TorcSystemNotification::TorcSystemNotification(const QVariantMap &Details)
  : TorcNotification(Details)
{
    if (uniqueId.isEmpty() || m_notifierNames.isEmpty() || m_body.isEmpty())
        return;

    // NB individual event strings are validated by the xsd.
    if (Details.contains("inputs"))
    {
        QVariantMap inputs = Details.value("inputs").toMap();
        QVariantMap::const_iterator it = inputs.constBegin();
        for ( ; it != inputs.constEnd(); ++it)
            if (it.key() == "event")
                m_events.append(it.value().toString().trimmed().toLower());
    }

    if (m_events.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("System notification '%1' has no events to listen for").arg(uniqueId));
    }
    else
    {
        gLocalContext->AddObserver(this);
        SetValid(true);
    }
}

TorcSystemNotification::~TorcSystemNotification()
{
    gLocalContext->RemoveObserver(this);
}

/// Listen for system events (TorcEvent).
bool TorcSystemNotification::event(QEvent *Event)
{
    if (Event && (Event->type() == TorcEvent::TorcEventType))
    {
        TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
        if (torcevent)
        {
            QMutexLocker locker(lock);
            QString eventname = Torc::ActionToString((Torc::Actions)torcevent->GetEvent()).toLower();
            if (m_events.contains(eventname))
            {
                QMap<QString,QString> custom;
                custom.insert("event", Torc::ActionToString((Torc::Actions)torcevent->GetEvent()));
                QVariantMap message = TorcNotify::gNotify->SetNotificationText(m_title, m_body, custom);
                emit Notify(message);
            }
        }
    }

    return TorcNotification::event(Event);
}

/// Create instances of TorcSystemNotification.
class TorcSystemNotificationFactory : public TorcNotificationFactory
{
    TorcNotification* Create(const QString &Type, const QVariantMap &Details)
    {
        if (Type == "system" && Details.contains("inputs"))
            return new TorcSystemNotification(Details);
        return NULL;
    }
} TorcSystemNotificationFactory;

