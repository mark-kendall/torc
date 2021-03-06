/* Class TorcLogNotifier
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
#include "torclognotifier.h"

/*! \class TorcLogNotifier
 *  \brief Log notifications to the log file.
 *
 * \note This notifier requires no additional parameters.
*/
TorcLogNotifier::TorcLogNotifier(const QVariantMap &Details)
  : TorcNotifier(Details)
{
    SetValid(true);
}

QStringList TorcLogNotifier::GetDescription(void)
{
    return QStringList() << tr("Log");
}

void TorcLogNotifier::Notify(const QVariantMap &Notification)
{
    QString message = Notification.contains(NOTIFICATION_BODY) ? Notification.value(NOTIFICATION_BODY).toString() : UNKNOWN_BODY;
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Notify: %1").arg(message));
}

class TorcLogNotifierFactory final : public TorcNotifierFactory
{
    TorcNotifier* Create(const QString &Type, const QVariantMap &Details) override
    {
        if (Type == QStringLiteral("log"))
            return new TorcLogNotifier(Details);
        return nullptr;
    }
} TorcLogNotifierFactory;

