/* Class TorcThingSpeakNotifier
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
#include "torcnetwork.h"
#include "torcthingspeaknotifier.h"

#define THINGSPEAK_UPDATE_URL QString("https://api.thingspeak.com/update")
#define THINGSPEAK_MAX_ERRORS 5

TorcThingSpeakNotifier::TorcThingSpeakNotifier(const QVariantMap &Details)
  : TorcNotifier(Details),
    m_timer(new QTimer()),
    m_networkAbort(0),
    m_apiKey(""),
    m_lastUpdate(QDateTime::fromMSecsSinceEpoch(0)),
    m_badRequestCount(0)
{
    // setup timer
    m_timer->setSingleShot(true);
    connect(this,    SIGNAL(StartTimer(int)), m_timer, SLOT(start(int)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(DoNotify()));

    for (int i = 0; i < 8; i++)
        m_fieldValues[i] = "";

    if (Details.contains("apikey"))
        m_apiKey = Details.value("apikey").toString().trimmed();

    if (m_apiKey.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "ThingSpeak notifier has no api key - disabling");
        return;
    }

    if (Details.contains("fields"))
    {
        QVariantMap fields = Details.value("fields").toMap();
        for (int i = 0; i < 8; i++)
        {
            QString field = "field" + QString::number(i +1);
            if (fields.contains(field))
            {
                QString value = fields.value(field).toString().trimmed();
                if (!value.isEmpty())
                    m_fields.insert(value, i);
            }
        }
    }

    if (m_fields.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "ThingSpeak notifier has no valid fields");
        return;
    }

    SetValid(true);
}

TorcThingSpeakNotifier::~TorcThingSpeakNotifier()
{
    QMutexLocker locker(lock);

    m_networkAbort = 1;

    foreach (TorcNetworkRequest* request, m_requests)
    {
        TorcNetwork::Cancel(request);
        request->DownRef();
    }
    m_requests.clear();

    delete m_timer;
}

QStringList TorcThingSpeakNotifier::GetDescription(void)
{
    QStringList result;
    result << tr("ThingSpeak");
    QMap<QString,int>::const_iterator it = m_fields.constBegin();
    for ( ; it != m_fields.constEnd(); ++it)
        result << QString("field%1: %2").arg(it.value() + 1).arg(it.key());
    return result;
}

void TorcThingSpeakNotifier::Notify(const QVariantMap &Notification)
{
    if (!GetValid())
        return;

    // set fields...
    bool updated = false;
    QMap<QString,int>::const_iterator it = m_fields.constBegin();
    for ( ; it != m_fields.constEnd(); ++it)
    {
        if (Notification.contains(it.key()))
        {
            m_fieldValues[it.value()] = Notification.value(it.key()).toString();
            updated = true;
        }
    }

    if (updated)
        DoNotify();
}

void TorcThingSpeakNotifier::DoNotify(void)
{
    if (!GetValid())
        return;

    if (!TorcNetwork::IsAvailable())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Not sending ThingSpeak update. Network is not available"));
        return;
    }

    QDateTime now = QDateTime::currentDateTime();

    // ThingSpeak limits updates to a max of 1 every 15 seconds. If we try and update too fast, simply
    // amalgamate fields and start the timer to trigger the next update. If the same field is updated more than
    // once, its value is overwritten with the most recent value. The timestamp may also be out of date for 'stale' fields.

    if (m_lastUpdate.secsTo(now) < 15)
    {
        if (m_timer->isActive())
            return;

        StartTimer(m_lastUpdate.msecsTo(now));
        return;
    }

    m_lastUpdate = now;

    QUrl url(THINGSPEAK_UPDATE_URL);
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey);
    for (int i = 0; i < 8; i++)
    {
        if (!m_fieldValues[i].isEmpty())
            query.addQueryItem(QString("field%1").arg(i+1), m_fieldValues[i]);
        // reset the fields
        m_fieldValues[i] = QString("");
    }
    url.setQuery(query);
    QNetworkRequest qrequest(url);

    TorcNetworkRequest *request = new TorcNetworkRequest(qrequest, QNetworkAccessManager::GetOperation, 0, &m_networkAbort);
    TorcNetwork::GetAsynchronous(request, this);

    {
        QMutexLocker locker(lock);
        m_requests.append(request);
    }
}

void TorcThingSpeakNotifier::RequestReady(TorcNetworkRequest *Request)
{
    if (!Request)
        return;

    QMutexLocker locker(lock);
    if (!m_requests.contains(Request))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Response to unknown ThingSpeak update");
        return;
    }

    int status = Request->GetStatus();
    if (status >= HTTP_BadRequest && status < HTTP_InternalServerError)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("ThingSpeak update error '%1'").arg(TorcHTTPRequest::StatusToString((HTTPStatus)status)));
        if (++m_badRequestCount > THINGSPEAK_MAX_ERRORS)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1 ThingSpeak update errors. Disabling notifier. Check your API key."));
            SetValid(false);
        }
    }
    else
    {
        // a successful update returns a non-zero update id - and zero on error.
        QByteArray result = Request->ReadAll(1000).trimmed();
        bool ok = false;
        quint64 id = result.toLongLong(&ok);
        if (ok)
        {
            if (id == 0)
                LOG(VB_GENERAL, LOG_ERR, QString("ThingSpeak update failed for '%1'").arg(uniqueId));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse update id from ThingSpeak response (%1)").arg(uniqueId));
        }
    }

    m_requests.removeAll(Request);
    Request->DownRef();
}

class TorcThingSpeakNotifierFactory : public TorcNotifierFactory
{
    TorcNotifier* Create(const QString &Type, const QVariantMap &Details)
    {
        if (Type == "thingspeak" && Details.contains("apikey") && Details.contains("fields"))
            return new TorcThingSpeakNotifier(Details);
        return NULL;
    }
} TorcThingSpeakNotifierFactory;



