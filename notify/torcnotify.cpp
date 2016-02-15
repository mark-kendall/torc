/* Class TorcNotify
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
    m_applicationNameChanged(false)
{
}

TorcNotify::~TorcNotify()
{
    // just in case
    Destroy();
}

bool TorcNotify::Validate(void) const
{
    // there isn't always an app object when gNotify is created, so connect the dots here but ensure
    // connections are unique to account for multiple runs.
    connect(qApp, SIGNAL(applicationNameChanged()), gNotify, SLOT(ApplicationNameChanged()), Qt::UniqueConnection);

    QMutexLocker locker(m_lock);

    foreach (TorcNotification* notification, m_notifications)
        (void)notification->Setup();

    return true;
}

TorcNotifier* TorcNotify::FindNotifierByName(const QString &Name) const
{
    QMutexLocker locker(m_lock);
    foreach (TorcNotifier* notifier, m_notifiers)
        if (notifier->GetUniqueId() == Name)
            return notifier;

    return NULL;
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
            data.insert("applicationname", QCoreApplication::applicationName());
            initialised = true;
        }
        {
            QMutexLocker locker(m_lock);
            m_applicationNameChanged = false;
        }
    }

    QDateTime datetime = QDateTime::currentDateTime();
    QTime         time = datetime.time();
    QDate         date = datetime.date();

    {
        QWriteLocker locker(lock);
        data.insert("datetime",      datetime.toString(Qt::TextDate));
        data.insert("shortdatetime", datetime.toString(Qt::SystemLocaleShortDate));
        data.insert("longdatetime",  datetime.toString(Qt::SystemLocaleLongDate));
        data.insert("time",          time.toString(Qt::TextDate));
        data.insert("shorttime",     time.toString(Qt::SystemLocaleShortDate));
        data.insert("longtime",      time.toString(Qt::SystemLocaleLongDate));
        data.insert("date",          date.toString(Qt::TextDate));
        data.insert("shortdate",     date.toString(Qt::SystemLocaleShortDate));
        data.insert("longdate",      date.toString(Qt::SystemLocaleLongDate));
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
                title.replace(regexp.cap(0), QString("%1%2%3%4").arg(regexp.cap(2)).arg(regexp.cap(3)).arg(data.value(key)).arg(regexp.cap(6)));
            pos += regexp.matchedLength();
        }
        pos = 0;
        while ((pos = regexp.indexIn(Body, pos)) != -1)
        {
            QString key = regexp.cap(4).toLower().trimmed();
            if (data.contains(key))
                body.replace(regexp.cap(0), QString("%1%2%3%4").arg(regexp.cap(2)).arg(regexp.cap(3)).arg(data.value(key)).arg(regexp.cap(6)));
            pos += regexp.matchedLength();
        }
    }
    // and custom data
    pos = 0;
    while ((pos = regexp.indexIn(Title, pos)) != -1)
    {
        QString key = regexp.cap(4).toLower().trimmed();
        if (Custom.contains(key))
            title.replace(regexp.cap(0), QString("%1%2%3%4").arg(regexp.cap(2)).arg(regexp.cap(3)).arg(Custom.value(key)).arg(regexp.cap(6)));
        pos += regexp.matchedLength();
    }
    pos = 0;
    while ((pos = regexp.indexIn(Body, pos)) != -1)
    {
        QString key = regexp.cap(4).toLower().trimmed();
        if (Custom.contains(key))
            body.replace(regexp.cap(0), QString("%1%2%3%4").arg(regexp.cap(2)).arg(regexp.cap(3)).arg(Custom.value(key)).arg(regexp.cap(6)));
        pos += regexp.matchedLength();
    }


    QVariantMap message;
    message.insert(NOTIFICATION_TITLE, title);
    message.insert(NOTIFICATION_BODY,  body);
    return message;
}

void TorcNotify::ApplicationNameChanged(void)
{
    QMutexLocker locker(m_lock);
    m_applicationNameChanged = true;
}

void TorcNotify::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    QVariantMap::const_iterator ii = Details.constBegin();
    for ( ; ii != Details.constEnd(); ii++)
    {
        if (ii.key() == "notify")
        {
            QVariantMap notify = ii.value().toMap();
            QVariantMap::const_iterator i = notify.constBegin();
            for ( ; i != notify.constEnd(); i++)
            {
                if (i.key() == "notifiers")
                {
                    QVariantMap notifiers = i.value().toMap();
                    QVariantMap::iterator it = notifiers.begin();
                    for ( ; it != notifiers.end(); ++it)
                    {
                        QVariantMap details = it.value().toMap();
                        if (!details.contains("name"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Notifier '%1' has no name").arg(it.key()));
                            continue;
                        }

                        TorcNotifier* notifier = NULL;
                        TorcNotifierFactory* factory = TorcNotifierFactory::GetTorcNotifierFactory();
                        for ( ; factory; factory = factory->NextFactory())
                        {
                            notifier = factory->Create(it.key(), details);
                            if (notifier)
                            {
                                m_notifiers.append(notifier);
                                LOG(VB_GENERAL, LOG_INFO, QString("New notifier '%1'").arg(notifier->GetUniqueId()));
                                break;
                            }
                        }
                    }
                }
                else if (i.key() == "notifications")
                {
                    QVariantMap notifications = i.value().toMap();
                    QVariantMap::iterator it = notifications.begin();
                    for ( ; it != notifications.end(); ++it)
                    {
                        QVariantMap details = it.value().toMap();
                        if (!details.contains("name"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Notification '%1' has no name").arg(it.key()));
                            continue;
                        }

                        if (!details.contains("outputs"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Notification '%1' does not specify 'outputs' (notififiers)").arg(it.key()));
                            continue;
                        }

                        if (!details.contains("message"))
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Notificaiton '%1' does not specify 'message'").arg(it.key()));
                            continue;
                        }

                        TorcNotification* notification = NULL;
                        TorcNotificationFactory* factory = TorcNotificationFactory::GetTorcNotificationFactory();
                        for ( ; factory; factory = factory->NextFactory())
                        {
                            notification = factory->Create(it.key(), details);
                            if (notification)
                            {
                                m_notifications.append(notification);
                                LOG(VB_GENERAL, LOG_INFO, QString("New notification '%1'").arg(notification->GetUniqueId()));
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
    QMutexLocker locker(m_lock);
    foreach (TorcNotifier* notifier, m_notifiers)
        notifier->DownRef();
    m_notifiers.clear();

    foreach (TorcNotification* notification, m_notifications)
        notification->DownRef();
    m_notifications.clear();
}
