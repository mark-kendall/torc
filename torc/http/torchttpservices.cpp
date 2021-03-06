/* Class TorcHTTPServices
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2015-18
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

// Qt
#include <QCoreApplication>
#include <QMetaObject>
#include <QMetaMethod>
#include <QObject>

// Torc
#include "torclocalcontext.h"
#include "torcserialiser.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchttpservices.h"
#include "torcwebsockettoken.h"
#include "torcupnp.h"

/*! \class TorcHTTPServices
 *  \brief Top level interface into services.
 *
 * A helper service that provides details to clients on the available services, return types, WebSocket
 * protocols etc. It also acts as the entry point for clients wishing to authenticate for a WebSocket connection
 * - who need to call GetWebSocketToken.
*/
TorcHTTPServices::TorcHTTPServices(TorcHTTPServer *Server)
  : QObject(),
    TorcHTTPService(this, QStringLiteral(""), QStringLiteral("services"), TorcHTTPServices::staticMetaObject, QStringLiteral("HandlersChanged")),
    serviceList(),
    returnFormats(),
    webSocketProtocols()
{
    connect(Server, &TorcHTTPServer::HandlersChanged, this, &TorcHTTPServices::HandlersChanged);

    TorcSerialiserFactory *factory = TorcSerialiserFactory::GetTorcSerialiserFactory();
    for ( int i = 0; factory; factory = factory->NextTorcSerialiserFactory(), i++)
    {
        QVariantMap item;
        item.insert(QStringLiteral("name"), factory->Description());
        item.insert(QStringLiteral("type"), factory->MimeType());
        returnFormats.append(item);
    }
}

QString TorcHTTPServices::GetVersion(void)
{
    int index = TorcHTTPServices::staticMetaObject.indexOfClassInfo("Version");
    return (index > -1) ? TorcHTTPServices::staticMetaObject.classInfo(index).value() : QStringLiteral("unknown");
}

QString TorcHTTPServices::GetUIName(void)
{
    return tr("Services");
}

QVariantMap TorcHTTPServices::ProcessRequest(const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated)
{
    // N.B. this returns a result associated with the actual state of the underlying websocket and
    // not what the browser etc think is the correct state.
    if (Method.endsWith(QStringLiteral("IsSecure")))
    {
        QVariant sec((bool)false);
        TorcWebSocket *connection = qobject_cast<TorcWebSocket*>(Connection);
        if (connection && connection->IsSecure())
            sec = true;

        QVariantMap result;
        QVariantMap secure;
        secure.insert(TORC_SECURE, sec);
        result.insert(QStringLiteral("result"), secure);
        return result;
    }

    return TorcHTTPService::ProcessRequest(Method, Parameters, Connection, Authenticated);
}


void TorcHTTPServices::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
{
    // handle own service
    if (!Request.GetMethod().isEmpty())
    {
        // we handle GetWebSocketToken/IsSecure manually as they need access to the authentication headers.
        // and they have no value internally (hence dummy implementations)
        QString method = Request.GetMethod();
        if (method == QStringLiteral("GetWebSocketToken") || method == QStringLiteral("IsSecure"))
        {
            HTTPRequestType type = Request.GetHTTPRequestType();
            if (type == HTTPOptions)
            {
                Request.SetStatus(HTTP_OK);
                Request.SetResponseType(HTTPResponseDefault);
                Request.SetAllowed(HTTPGet | HTTPOptions);
            }
            else if (type == HTTPGet)
            {
                QVariant result;
                QString type;
                if (method == QStringLiteral("GetWebSocketToken"))
                {
                    // force authentication
                    if (!TorcHTTPHandler::MethodIsAuthorised(Request, HTTPAuth))
                        return;
                    result = TorcWebSocketToken::GetWebSocketToken(PeerAddress);
                    type   = QStringLiteral("accesstoken");
                }
                else if (method == QStringLiteral("IsSecure"))
                {
                    result = Request.GetSecure();
                    type   = TORC_SECURE;
                }

                Request.SetStatus(HTTP_OK);
                Request.Serialise(result, type);
            }
            else
            {
                Request.SetStatus(HTTP_BadRequest);
                Request.SetResponseType(HTTPResponseDefault);
            }

            return;
        }

        TorcHTTPService::ProcessHTTPRequest(PeerAddress, PeerPort, LocalAddress, LocalPort, Request);
        return;
    }

    // handle options request
    if (Request.GetHTTPRequestType() == HTTPOptions)
    {
        HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        return;
    }

    Request.SetStatus(HTTP_NotFound);
    Request.SetResponseType(HTTPResponseDefault);
}

void TorcHTTPServices::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Return complete application information.
 *
 * This acts as a convenience method for peers to retrieve pertinant application
 * information with one remote call.
*/
QVariantMap TorcHTTPServices::GetDetails(void)
{
    // NB keys here match those of the relevant stand alone methods. Take care not to break them.
    QVariantMap results;

    results.insert(TORC_SERVICE_VERSION, TorcHTTPServices::GetVersion());
    results.insert(QStringLiteral("services"), GetServiceList());
    results.insert(TORC_STARTTIME, GetStartTime());
    results.insert(TORC_PRIORITY,  GetPriority());
    results.insert(TORC_UUID,      GetUuid());

    return results;
}

QVariantMap TorcHTTPServices::GetServiceList(void)
{
    return TorcHTTPServer::GetServiceHandlers();
}

QVariantMap TorcHTTPServices::GetServiceDescription(const QString &Service)
{
    // the server has the services - and can lock the list
    return TorcHTTPServer::GetServiceDescription(Service);
}

QVariantList TorcHTTPServices::GetReturnFormats(void)
{
    return returnFormats;
}

QVariantList TorcHTTPServices::GetWebSocketProtocols(void)
{
    return TorcWebSocket::GetSupportedSubProtocols();
}

qint64 TorcHTTPServices::GetStartTime(void)
{
    return gLocalContext->GetStartTime();
}

int TorcHTTPServices::GetPriority(void)
{
    return gLocalContext->GetPriority();
}

QString TorcHTTPServices::GetUuid(void)
{
    return gLocalContext->GetUuid();
}

/*! \brief Return a WebSocket token for connecting a WebSocket when authentication is required.
 *
 * \note This is a dummy method as there is no point in calling it internally. The actual implementation
 *       is captured in ProcessHTTPRequest as it requires access to the underlying HTTP headers to authenticate.
*/
QString TorcHTTPServices::GetWebSocketToken(void)
{
    return QStringLiteral("");
}

bool TorcHTTPServices::IsSecure(void)
{
    return false; // dummy - as for GetWebSocketToken
}

void TorcHTTPServices::HandlersChanged(void)
{
    emit ServiceListChanged();
}
