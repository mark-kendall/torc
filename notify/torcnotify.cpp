/* Class TorcNotify
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

// Qt
#include <QCoreApplication>
#include <QReadWriteLock>
#include <QDateTime>

// Torc
#include "torclogging.h"
#include "torcnotify.h"

TorcNotify* TorcNotify::gNotify = new TorcNotify();

TorcNotify::TorcNotify()
  : QObject(),
    TorcDeviceHandler(),
    m_notifiers(),
    m_notifications(),
    m_applicationNameChanged(false)
{
}

bool TorcNotify::Validate(void)
{
    // there isn't always an app object when gNotify is created, so connect the dots here but ensure
    // connections are unique to account for multiple runs.
    connect(qApp, &QCoreApplication::applicationNameChanged, gNotify, &TorcNotify::ApplicationNameChanged, Qt::UniqueConnection);

    QWriteLocker locker(&m_handlerLock);

    foreach (TorcNotification* notification, m_notifications)
        (void)notification->Setup();

    return true;
}

TorcNotifier* TorcNotify::FindNotifierByName(const QString &Name)
{
    QWriteLocker locker(&m_handlerLock);
    foreach (TorcNotifier* notifier, m_notifiers)
        if (notifier->GetUniqueId() == Name)
            return notifier;

    return nullptr;
}

void TorcNotify::Graph(QByteArray *Data)
{
    if (!Data)
        return;

    foreach(TorcNotification *notification, m_notifications)
    {
        QString id    = notification->GetUniqueId();
        QString label = notification->GetUserName();
        QString desc;
        QStringList source = notification->GetDescription();
        foreach (const QString &item, source)
            if (!item.isEmpty())
                desc.append(QString(DEVICE_LINE_ITEM).arg(item));

        if (label.isEmpty())
            label = id;
        Data->append(QStringLiteral("        \"%1\" [shape=record style=rounded id=\"%1\" label=<<B>%2</B>%3>];\r\n").arg(id, label, desc));

        // and add links
        notification->Graph(Data);
    }

    foreach(TorcNotifier *notifier, m_notifiers)
    {
        QString id    = notifier->GetUniqueId();
        QString label = notifier->GetUserName();
        QString desc;
        QStringList source = notifier->GetDescription();
        foreach (const QString &item, source)
            desc.append(QString(DEVICE_LINE_ITEM).arg(item));

        if (label.isEmpty())
            label = id;
        Data->append(QStringLiteral("        \"%1\" [shape=record style=rounded id=\"%1\" label=<<B>%2</B>%3>];\r\n").arg(id, label, desc));
    }
}

QVariantMap TorcNotify::SetNotificationText(const QString &Title, const QString &Body, const QMap<QString,QString> &Custom)
{
    static bool initialised = false;
    static QMap<QString,QString> data;
    static QReadWriteLock *lock = new QReadWriteLock(QReadWriteLock::Recursive);

    if (!initialised || m_applicationNameChanged == true)
    {
        {
            QWriteLocker locker(lock);
            data.insert(QStringLiteral("applicationname"), QCoreApplication::applicationName());
            initialised = true;
        }
        {
            QWriteLocker locker(&m_handlerLock);
            m_applicationNameChanged = false;
        }
    }

    QDateTime datetime = QDateTime::currentDateTime();
    QTime         time = datetime.time();
    QDate         date = datetime.date();

    {
        QWriteLocker locker(lock);
        data.insert(QStringLiteral("datetime"),      datetime.toString(Qt::TextDate));
        data.insert(QStringLiteral("shortdatetime"), datetime.toString(Qt::SystemLocaleShortDate));
        data.insert(QStringLiteral("longdatetime"),  datetime.toString(Qt::SystemLocaleLongDate));
        data.insert(QStringLiteral("time"),          time.toString(Qt::TextDate));
        data.insert(QStringLiteral("shorttime"),     time.toString(Qt::SystemLocaleShortDate));
        data.insert(QStringLiteral("longtime"),      time.toString(Qt::SystemLocaleLongDate));
        data.insert(QStringLiteral("date"),          date.toString(Qt::TextDate));
        data.insert(QStringLiteral("shortdate"),     date.toString(Qt::SystemLocaleShortDate));
        data.insert(QStringLiteral("longdate"),      date.toString(Qt::SystemLocaleLongDate));
    }

    QRegExp regexp("%(([^\\|%]+)?\\||\\|(.))?([\\w#]+)(\\|(.+))?%");
    regexp.setMinimal(true);

    // modify title and body for 'global' data
    QString title = Title;
    QString body  = Body;
    int pos = 0;
    {
        QReadLocker locker(lock);

        while ((pos = regexp.indexIn(Title, pos)) != -1)
        {
            QString key = regexp.cap(4).toLower().trimmed();
            if (data.contains(key))
                title.replace(regexp.cap(0), QStringLiteral("%1%2%3%4").arg(regexp.cap(2), regexp.cap(3), data.value(key), regexp.cap(6)));
            pos += regexp.matchedLength();
        }
        pos = 0;
        while ((pos = regexp.indexIn(Body, pos)) != -1)
        {
            QString key = regexp.cap(4).toLower().trimmed();
            if (data.contains(key))
                body.replace(regexp.cap(0), QStringLiteral("%1%2%3%4").arg(regexp.cap(2), regexp.cap(3), data.value(key), regexp.cap(6)));
            pos += regexp.matchedLength();
        }
    }
    // and custom data
    pos = 0;
    while ((pos = regexp.indexIn(Title, pos)) != -1)
    {
        QString key = regexp.cap(4).toLower().trimmed();
        if (Custom.contains(key))
            title.replace(regexp.cap(0), QStringLiteral("%1%2%3%4").arg(regexp.cap(2), regexp.cap(3), Custom.value(key), regexp.cap(6)));
        pos += regexp.matchedLength();
    }
    pos = 0;
    while ((pos = regexp.indexIn(Body, pos)) != -1)
    {
        QString key = regexp.cap(4).toLower().trimmed();
        if (Custom.contains(key))
            body.replace(regexp.cap(0), QStringLiteral("%1%2%3%4").arg(regexp.cap(2), regexp.cap(3), Custom.value(key), regexp.cap(6)));
        pos += regexp.matchedLength();
    }


    QVariantMap message;
    message.insert(NOTIFICATION_TITLE, title);
    message.insert(NOTIFICATION_BODY,  body);

    QMap<QString,QString>::const_iterator it = Custom.constBegin();
    for ( ; it != Custom.constEnd(); ++it)
        message.insert(it.key(), it.value());
    return message;
}

void TorcNotify::ApplicationNameChanged(void)
{
    QWriteLocker locker(&m_handlerLock);
    m_applicationNameChanged = true;
}

void TorcNotify::Create(const QVariantMap &Details)
{
    QWriteLocker locker(&m_handlerLock);

    QVariantMap::const_iterator ii = Details.constBegin();
    for ( ; ii != Details.constEnd(); ++ii)
    {
        if (ii.key() == QStringLiteral("notify"))
        {
            QVariantMap notify = ii.value().toMap();
            QVariantMap::const_iterator i = notify.constBegin();
            for ( ; i != notify.constEnd(); ++i)
            {
                if (i.key() == QStringLiteral("notifiers"))
                {
                    QVariantMap notifiers = i.value().toMap();
                    QVariantMap::iterator it = notifiers.begin();
                    for ( ; it != notifiers.end(); ++it)
                    {
                        QVariantMap details = it.value().toMap();
                        if (!details.contains(QStringLiteral("name")))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Notifier '%1' has no name").arg(it.key()));
                            continue;
                        }

                        TorcNotifierFactory* factory = TorcNotifierFactory::GetTorcNotifierFactory();
                        for ( ; factory; factory = factory->NextFactory())
                        {
                            TorcNotifier* notifier = factory->Create(it.key(), details);
                            if (notifier)
                            {
                                m_notifiers.append(notifier);
                                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("New notifier '%1'").arg(notifier->GetUniqueId()));
                                break;
                            }
                        }
                    }
                }
                else if (i.key() == QStringLiteral("notifications"))
                {
                    QVariantMap notifications = i.value().toMap();
                    QVariantMap::iterator it = notifications.begin();
                    for ( ; it != notifications.end(); ++it)
                    {
                        QVariantMap details = it.value().toMap();
                        if (!details.contains(QStringLiteral("name")))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Notification '%1' has no name").arg(it.key()));
                            continue;
                        }

                        if (!details.contains(QStringLiteral("outputs")))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Notification '%1' does not specify 'outputs' (notififiers)").arg(it.key()));
                            continue;
                        }

                        if (!details.contains(QStringLiteral("message")))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Notificaiton '%1' does not specify 'message'").arg(it.key()));
                            continue;
                        }

                        TorcNotificationFactory* factory = TorcNotificationFactory::GetTorcNotificationFactory();
                        for ( ; factory; factory = factory->NextFactory())
                        {
                            TorcNotification* notification = factory->Create(it.key(), details);
                            if (notification)
                            {
                                m_notifications.append(notification);
                                LOG(VB_GENERAL, LOG_INFO, QStringLiteral("New notification '%1'").arg(notification->GetUniqueId()));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void TorcNotify::Destroy(void)
{
    QWriteLocker locker(&m_handlerLock);
    foreach (TorcNotifier* notifier, m_notifiers)
        notifier->DownRef();
    m_notifiers.clear();

    foreach (TorcNotification* notification, m_notifications)
        notification->DownRef();
    m_notifications.clear();
}
