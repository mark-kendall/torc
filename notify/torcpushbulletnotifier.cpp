/* Class TorcPushbulletNotifier
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
#include "torclogging.h"
#include "torchttprequest.h"
#include "torcnetworkrequest.h"
#include "torcpushbulletnotifier.h"

#define MAX_BAD_REQUESTS 2

/*! \class TorcPushbulletNotifier
 *  \brief Send push notifications using the Pushbullet API.
 *
 * \param Details Data required for the API (i.e. Acccess-Token)
*/
TorcPushbulletNotifier::TorcPushbulletNotifier(const QVariantMap &Details)
  : TorcNotifier(Details),
    m_resetTimer(new QTimer()),
    m_networkAbort(0),
    m_accessToken(QString("")),
    m_badRequestCount(0)
{
    // setup timer
    m_resetTimer->setSingleShot(true);
    connect(this, SIGNAL(StartResetTimer(int)), m_resetTimer, SLOT(start(int)));
    connect(m_resetTimer, SIGNAL(timeout()), this, SLOT(ResetTimerTimeout()));

    if (Details.contains("accesstoken"))
    {
        m_accessToken = Details.value("accesstoken").toString();
        SetValid(true);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "No Pushbullet access token specified - disabling");
        m_badRequestCount = MAX_BAD_REQUESTS;
    }
}

TorcPushbulletNotifier::~TorcPushbulletNotifier()
{
    QMutexLocker locker(lock);

    m_networkAbort = 1;

    foreach (TorcNetworkRequest* request, m_requests)
    {
        TorcNetwork::Cancel(request);
        request->DownRef();
    }
    m_requests.clear();

    delete m_resetTimer;
}

QStringList TorcPushbulletNotifier::GetDescription(void)
{
    return QStringList() << tr("Pushbullet");
}

void TorcPushbulletNotifier::Notify(const QVariantMap &Notification)
{
    if (m_accessToken.isEmpty() || m_badRequestCount >= MAX_BAD_REQUESTS || m_resetTimer->isActive())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Ignoring Pushbullet notify request");
        return;
    }

    if (!TorcNetwork::IsAvailable())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Not sending Pushbullet notification. Network is not available"));
        return;
    }

    QString title = Notification.contains(NOTIFICATION_TITLE) ? Notification.value(NOTIFICATION_TITLE).toString() : UNKNOWN_TITLE;
    QString body  = Notification.contains(NOTIFICATION_BODY)  ? Notification.value(NOTIFICATION_BODY).toString()  : UNKNOWN_BODY;

    QJsonObject object;
    object.insert("title", title);
    object.insert("body",  body);
    object.insert("type",  "note");
    QJsonDocument doc(object);
    QByteArray content = doc.toJson(QJsonDocument::Compact);

    QNetworkRequest qrequest(PUSHBULLET_PUSH_URL);
    qrequest.setRawHeader("Access-token", m_accessToken.toUtf8());
    qrequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    qrequest.setHeader(QNetworkRequest::ContentLengthHeader, content.size());

    TorcNetworkRequest *request = new TorcNetworkRequest(qrequest, content, &m_networkAbort);
    TorcNetwork::GetAsynchronous(request, this);

    {
        QMutexLocker locker(lock);
        m_requests.append(request);
    }
}

void TorcPushbulletNotifier::RequestReady(TorcNetworkRequest *Request)
{
    if (!Request)
        return;

    QMutexLocker locker(lock);
    if (!m_requests.contains(Request))
    {
        LOG(VB_GENERAL, LOG_ERR, "Response to unknown Pushbullet request");
        return;
    }

    int status = Request->GetStatus();
    if (status >= HTTP_BadRequest && status < HTTP_InternalServerError)
    {
        quint64 resetin = 3600; // default to restarting in 1 hour if needed

        if (status == HTTP_TooManyRequests)
        {
            QByteArray resettime = Request->GetHeader("X-Ratelimit-Reset");

            bool ok = false;
            if (!resettime.isEmpty())
            {
                quint64 reset = resettime.toLongLong(&ok);
                if (ok)
                {
                    quint64 now = (QDateTime::currentMSecsSinceEpoch() + 500) / 1000;
                    if (reset >= now)
                        resetin = reset - now;
                }
            }

            LOG(VB_GENERAL, LOG_WARNING, QString("Pushbullet rate limit exceeded. Restarting in %1 seconds").arg(resetin));
        }
        else
        {
            QJsonDocument result = QJsonDocument::fromJson(Request->GetBuffer());
            LOG(VB_GENERAL, LOG_ERR, QString("Pushbullet notifier '%1' - bad request ('%2')").arg(uniqueId).arg(TorcHTTPRequest::StatusToString((HTTPStatus)status)));
            LOG(VB_GENERAL, LOG_ERR, QString("Pushbullet replied with:\r\n%1").arg(result.toJson().constData()));
            if (++m_badRequestCount >= MAX_BAD_REQUESTS)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Disabling Pushbullet notifier '%1' - too many bad requests. Check your access token.").arg(uniqueId));
                LOG(VB_GENERAL, LOG_ERR, QString("Will try again in %1 seconds").arg(resetin));
            }
            else
            {
                resetin = 0;
            }
        }

        if (resetin > 0)
            emit StartResetTimer(resetin * 1000);
    }
    else
    {
        m_badRequestCount = 0;
    }

    Request->DownRef();
    m_requests.removeAll(Request);
}

void TorcPushbulletNotifier::ResetTimerTimeout(void)
{
    LOG(VB_GENERAL, LOG_INFO, QString("Re-enabling Pushbullet notifier '%1'").arg(uniqueId));
    m_badRequestCount = 0;
    SetValid(!m_accessToken.isEmpty());
}

class TorcPushbulletNotifierFactory : public TorcNotifierFactory
{
    TorcNotifier* Create(const QString &Type, const QVariantMap &Details)
    {
        if (Type == "pushbullet" && Details.contains("accesstoken"))
            return new TorcPushbulletNotifier(Details);
        return NULL;
    }
} TorcPushbulletNotifierFactory;

