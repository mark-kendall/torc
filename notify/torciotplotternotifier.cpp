/* Class TorcIoTPlotterNotifier
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
#include "torclogging.h"
#include "torchttprequest.h"
#include "torcnetwork.h"
#include "torciotplotternotifier.h"

#define IOTPLOTTER_UPDATE_URL QString("http://iotplotter.com/api/v2/feed/") // NB insecure
#define IOTPLOTTER_MAX_ERRORS 5
#define IOTPLOTTER_MAX_FIELDS 8 // I can't actually see a max

TorcIoTPlotterNotifier::TorcIoTPlotterNotifier(const QVariantMap &Details)
  : TorcIOTLogger(Details),
    m_feedId()
{
    m_description    = tr("IoTPlotter");
    m_maxBadRequests = IOTPLOTTER_MAX_ERRORS;
    m_maxFields      = IOTPLOTTER_MAX_FIELDS;

    // as well as an api key, IoTPlotter needs a feed id
    if (Details.contains("feedid"))
        m_feedId = Details.value("feedid").toString().trimmed();

    if (m_feedId.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 logger has no feed id - disabling").arg(m_description));
        return;
    }

    SetValid(Initialise(Details));
}

TorcIoTPlotterNotifier::~TorcIoTPlotterNotifier()
{
}

TorcNetworkRequest* TorcIoTPlotterNotifier::CreateRequest(void)
{
    QUrl url(IOTPLOTTER_UPDATE_URL + m_feedId);
    QNetworkRequest qrequest(url);
    qrequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    qrequest.setRawHeader("api-key", m_apiKey.toLocal8Bit());

    QJsonObject data;
    QJsonObject raw;
    // NB from Qt 5.8 we can use currentSecsSinceEpoch
    QString epoch = QString("%1").arg(((quint64)QDateTime::currentMSecsSinceEpoch() / 1000));

    for (int i = 0; i < m_maxFields; i++)
    {
        if (!m_fieldValues[i].isEmpty())
        {
            if (m_reverseFields.contains(i))
            {
                QJsonObject pair;
                pair.insert("value", 55);//m_fieldValues[i].toLongLong());
                //pair.insert("epoch", epoch.toLongLong());
                QJsonArray values;
                values.append(pair);
                raw.insert(m_reverseFields[i].toLocal8Bit(), values);
            }
        }

        // reset the fields
        m_fieldValues[i] = QString("");
    }

    data.insert("data", raw);
    QJsonDocument doc(data);
    QByteArray serialiseddata = doc.toJson(QJsonDocument::Indented);
    qrequest.setHeader(QNetworkRequest::ContentLengthHeader, serialiseddata.size());
    return new TorcNetworkRequest(qrequest, serialiseddata, &m_networkAbort);
}

void TorcIoTPlotterNotifier::ProcessRequest(TorcNetworkRequest *Request)
{
    // IoTPlotter returns a simple HTTP code
    (void)Request;
}

class TorcIoTPlotterNotifierFactory : public TorcNotifierFactory
{
    TorcNotifier* Create(const QString &Type, const QVariantMap &Details)
    {
        if (Type == "iotplotter" && Details.contains("apikey") && Details.contains("fields") &&
            Details.contains("feedid"))
        {
            return new TorcIoTPlotterNotifier(Details);
        }
        return NULL;
    }
} TorcIoTPlotterNotifierFactory;
