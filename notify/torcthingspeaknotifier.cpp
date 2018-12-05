/* Class TorcThingSpeakNotifier
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
#include "torchttprequest.h"
#include "torcnetwork.h"
#include "torcthingspeaknotifier.h"

#define THINGSPEAK_UPDATE_URL QStringLiteral("https://api.thingspeak.com/update")
#define THINGSPEAK_MAX_ERRORS 5
#define THINGSPEAK_MAX_FIELDS 8

TorcThingSpeakNotifier::TorcThingSpeakNotifier(const QVariantMap &Details)
  : TorcIOTLogger(Details)
{
    m_description    = tr("ThingSpeak");
    m_maxBadRequests = THINGSPEAK_MAX_ERRORS;
    m_maxFields      = THINGSPEAK_MAX_FIELDS;

    SetValid(Initialise(Details));
}

void TorcThingSpeakNotifier::ProcessRequest(TorcNetworkRequest *Request)
{
    if (!Request)
        return;

    // a successful update returns a non-zero update id - and zero on error.
    QByteArray result = Request->ReadAll(1000).trimmed();
    bool ok = false;
    quint64 id = result.toLongLong(&ok);
    if (ok)
    {
        if (id == 0)
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("%1 update failed for '%2'").arg(m_description, uniqueId));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to parse update id from %1 response (%2)").arg(m_description, uniqueId));
    }
}

TorcNetworkRequest* TorcThingSpeakNotifier::CreateRequest(void)
{
    QUrl url(THINGSPEAK_UPDATE_URL);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    for (int i = 0; i < m_maxFields; i++)
    {
        if (!m_fieldValues[i].isEmpty())
            query.addQueryItem(QStringLiteral("field%1").arg(i+1), m_fieldValues[i]);
        // reset the fields
        m_fieldValues[i] = QStringLiteral("");
    }
    url.setQuery(query);
    QNetworkRequest qrequest(url);

    return new TorcNetworkRequest(qrequest, QNetworkAccessManager::GetOperation, 0, &m_networkAbort);
}

class TorcThingSpeakNotifierFactory final : public TorcNotifierFactory
{
    TorcNotifier* Create(const QString &Type, const QVariantMap &Details) override
    {
        if (Type == QStringLiteral("thingspeak") && Details.contains(QStringLiteral("apikey")) && Details.contains(QStringLiteral("fields")))
            return new TorcThingSpeakNotifier(Details);
        return nullptr;
    }
} TorcThingSpeakNotifierFactory;



