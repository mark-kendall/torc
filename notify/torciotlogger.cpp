/* Class TorcIOTLogger
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
#include "torciotlogger.h"
#include "torcnetwork.h"
#include "torclogging.h"
#include "torchttprequest.h"

TorcIOTLogger::TorcIOTLogger(const QVariantMap &Details)
  : TorcNotifier(Details),
    m_description("IOTLogger"),
    m_timer(),
    m_networkAbort(0),
    m_apiKey(""),
    m_badRequestCount(0),
    m_maxBadRequests(5),
    m_maxUpdateInterval(15),
    m_lastUpdate(QDateTime::fromMSecsSinceEpoch(0)),
    m_requests(),
    m_maxFields(32),
    m_fields(),
    m_reverseFields()

{
    // clear fields
    for (int i = 0; i < 32; i++)
        m_fieldValues[i] = "";
}

TorcIOTLogger::~TorcIOTLogger()
{
    QMutexLocker locker(&lock);

    m_networkAbort = 1;

    foreach (TorcNetworkRequest* request, m_requests)
    {
        TorcNetwork::Cancel(request);
        request->DownRef();
    }
    m_requests.clear();
}

bool TorcIOTLogger::Initialise(const QVariantMap &Details)
{
    if (Details.contains("apikey"))
        m_apiKey = Details.value("apikey").toString().trimmed();

    if (m_apiKey.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 logger has no api key - disabling").arg(m_description));
        return false;
    }

    if (Details.contains("fields"))
    {
        QVariantMap fields = Details.value("fields").toMap();
        for (int i = 0; i < m_maxFields; i++)
        {
            QString field = "field" + QString::number(i +1);
            if (fields.contains(field))
            {
                QString value = fields.value(field).toString().trimmed();
                if (!value.isEmpty())
                {
                    m_fields.insert(value, i);
                    m_reverseFields.insert(i, value);
                }
            }
        }
    }

    if (m_fields.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 logger has no valid fields - disabling").arg(m_description));
        return false;
    }

    // at this point, all should be good. Set up the timer
    connect(this, SIGNAL(TryNotify()), this, SLOT(DoNotify()));
    m_timer.setSingleShot(true);
    connect(this,    SIGNAL(StartTimer(int)), &m_timer, SLOT(start(int)));
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(DoNotify()));

    return true;
}

QStringList TorcIOTLogger::GetDescription(void)
{
    QStringList result;
    result << m_description;
    QMap<QString,int>::const_iterator it = m_fields.constBegin();
    for ( ; it != m_fields.constEnd(); ++it)
        result << QString("field%1: %2").arg(it.value() + 1).arg(it.key());
    return result;
}

void TorcIOTLogger::Notify(const QVariantMap &Notification)
{
    if (!GetValid())
        return;

    bool updated = false;
    // set fields...
    {
        QMutexLocker locker(&lock);

        QMap<QString,int>::const_iterator it = m_fields.constBegin();
        for ( ; it != m_fields.constEnd(); ++it)
        {
            if (Notification.contains(it.key()))
            {
                m_fieldValues[it.value()] = Notification.value(it.key()).toString();
                updated = true;
            }
        }
    }

    if (updated)
        emit TryNotify();
}

void TorcIOTLogger::DoNotify(void)
{
    if (!GetValid())
        return;

    if (!TorcNetwork::IsAvailable())
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Not sending %1 update. Network is not available").arg(m_description));
        return;
    }

    QDateTime now = QDateTime::currentDateTime();

    // ThingSpeak/IOTPlotter etc limit updates to a max of 1 every 15 seconds. If we try and update too fast, simply
    // amalgamate fields and start the timer to trigger the next update. If the same field is updated more than
    // once, its value is overwritten with the most recent value. The timestamp may also be out of date for 'stale' fields.

    if (m_lastUpdate.secsTo(now) < m_maxUpdateInterval)
    {
        if (m_timer.isActive())
            return;

        StartTimer(m_lastUpdate.msecsTo(now));
        return;
    }

    m_lastUpdate = now;

    TorcNetworkRequest* request = CreateRequest();
    if (request)
    {
        TorcNetwork::GetAsynchronous(request, this);
        {
            QMutexLocker locker(&lock);
            m_requests.append(request);
        }
    }
}

void TorcIOTLogger::RequestReady(TorcNetworkRequest *Request)
{
    if (!Request)
        return;

    QMutexLocker locker(&lock);
    if (!m_requests.contains(Request))
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("Response to unknown %1 request").arg(m_description));
        return;
    }

    int status = Request->GetStatus();
    if (status >= HTTP_BadRequest)
    {
        // not sure whether it is my network or the thingspeak server - but I get intermittent Host Not Found errors
        // which end up disabling ThingSpeak. So check for HostNotFound...
        LOG(VB_GENERAL, LOG_ERR, QString("%2 update error '%1'").arg(TorcHTTPRequest::StatusToString((HTTPStatus)status)).arg(m_description));

        if (Request->GetReplyError() != QNetworkReply::HostNotFoundError)
        {
            if (++m_badRequestCount > m_maxBadRequests)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("%1 %2 update errors. Disabling logger - Check your config.")
                    .arg(m_badRequestCount)
                    .arg(m_description));
                SetValid(false);
            }
        }
    }
    else
    {
        // reset error count
        m_badRequestCount = 0;
        ProcessRequest(Request);
    }

    m_requests.removeAll(Request);
    Request->DownRef();
}
