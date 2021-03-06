/* Class TorcNotification
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torclogging.h"
#include "torcnotify.h"
#include "torcnotification.h"

/*! \page notifications Notifications
 * Notifications trigger sending messages of interest to external applications using \link notifiers notifiers \endlink.
 *
 * \class TorcNotification
 *
 * TorcNotification is the base notification class.
*/
TorcNotification::TorcNotification(const QVariantMap &Details)
  : TorcDevice(false, 0, 0, QStringLiteral("Notification"), Details),
    m_notifierNames(),
    m_notifiers(),
    m_title(),
    m_body()
{
    if (Details.contains(QStringLiteral("outputs")) && Details.contains(QStringLiteral("message")))
    {
        QVariantMap message = Details.value(QStringLiteral("message")).toMap();
        if (message.contains(QStringLiteral("body")))
        {
            m_body = message.value(QStringLiteral("body")).toString();
            // title is optional
            if (message.contains(QStringLiteral("title")))
                m_title = message.value(QStringLiteral("title")).toString();

            QVariantMap outputs = Details.value(QStringLiteral("outputs")).toMap();
            QVariantMap::const_iterator it = outputs.constBegin();
            for (; it != outputs.constEnd(); ++it)
                if (it.key() == QStringLiteral("notifier"))
                    m_notifierNames.append(it.value().toString().trimmed());
        }
    }

    if (m_notifierNames.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Notifier is incomplete"));
}

bool TorcNotification::Setup(void)
{
    QMutexLocker locker(&lock);

    if (uniqueId.isEmpty())
        return false;

    foreach (QString notifiername, m_notifierNames)
    {
        TorcNotifier* notifier = TorcNotify::gNotify->FindNotifierByName(notifiername);
        if (notifier)
        {
            connect(this,&TorcNotification::Notify, notifier, &TorcNotifier::Notify);
            m_notifiers.append(notifier);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Notification '%1' failed to find notifier '%2'").arg(uniqueId, notifiername));
        }
    }

    return true;
}

bool TorcNotification::IsKnownInput(const QString &UniqueId)
{
    (void)UniqueId;
    return false;
}

TorcNotificationFactory* TorcNotificationFactory::gTorcNotificationFactory = nullptr;

TorcNotificationFactory::TorcNotificationFactory()
  : nextTorcNotificationFactory(gTorcNotificationFactory)
{
    gTorcNotificationFactory = this;
}

TorcNotificationFactory* TorcNotificationFactory::GetTorcNotificationFactory(void)
{
    return gTorcNotificationFactory;
}

TorcNotificationFactory* TorcNotificationFactory::NextFactory(void) const
{
    return nextTorcNotificationFactory;
}


