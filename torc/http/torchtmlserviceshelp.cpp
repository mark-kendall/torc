/* Class TorcHTMLServicesHelp
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013
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
#include "torchttpconnection.h"
#include "torchtmlserviceshelp.h"

/*! \class TorcHTMLServicesHelp
 *  \brief Top level interface into services.
 *
 * A helper service that provides details to clients on the available services, return types, WebSocket
 * protocols etc. It also acts as the entry point for clients wishing to authenticate for a WebSocket connection
 * - who need to call GetWebSocketToken.
 *
 * \note This service no longer serves up any HTML, or help, so needs to be renamed to TorcHTTPServices
*/
TorcHTMLServicesHelp::TorcHTMLServicesHelp(TorcHTTPServer *Server)
  : QObject(),
    TorcHTTPService(this, "", "services", TorcHTMLServicesHelp::staticMetaObject, QString("HandlersChanged"))
{
    connect(Server, SIGNAL(HandlersChanged()), this, SLOT(HandlersChanged()));

    TorcSerialiserFactory *factory = TorcSerialiserFactory::GetTorcSerialiserFactory();
    for ( int i = 0; factory; factory = factory->NextTorcSerialiserFactory(), i++)
    {
        QVariantMap item;
        item.insert("name", factory->Description());
        item.insert("type", factory->Accepts());
        returnFormats.append(item);
    }
}

TorcHTMLServicesHelp::~TorcHTMLServicesHelp()
{
}

QString TorcHTMLServicesHelp::GetUIName(void)
{
    return tr("Services");
}

void TorcHTMLServicesHelp::ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection* Connection)
{
    if (!Request)
        return;

    // handle own service
    if (!Request->GetMethod().isEmpty())
    {
        // we handle GetWebSocketToken manually as it needs access to the authentication headers.`
        if (Request->GetMethod() == "GetWebSocketToken")
        {
            HTTPRequestType type = Request->GetHTTPRequestType();
            if (type == HTTPOptions)
            {
                Request->SetStatus(HTTP_OK);
                Request->SetResponseType(HTTPResponseDefault);
                Request->SetAllowed(HTTPGet | HTTPOptions);
            }
            else if (type == HTTPGet)
            {
                Request->SetStatus(HTTP_OK);
                TorcSerialiser *serialiser = Request->GetSerialiser();
                Request->SetResponseType(serialiser->ResponseType());
                Request->SetResponseContent(serialiser->Serialise(Connection->GetServer()->GetWebSocketToken(Connection, Request), "accesstoken"));
                delete serialiser;
            }
            else
            {
                Request->SetStatus(HTTP_BadRequest);
                Request->SetResponseType(HTTPResponseDefault);
            }

            return;
        }

        TorcHTTPService::ProcessHTTPRequest(Request, Connection);
        return;
    }

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseDefault);
        Request->SetResponseContent(NULL);
        return;
    }

    Request->SetStatus(HTTP_NotFound);
    Request->SetResponseType(HTTPResponseDefault);
}

void TorcHTMLServicesHelp::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

/*! \brief Return complete application information.
 *
 * This acts as a convenience method for peers to retrieve pertinant application
 * information with one remote call.
*/
QVariantMap TorcHTMLServicesHelp::GetDetails(void)
{
    // NB keys here match those of the relevant stand alone methods. Take care not to break them.
    QVariantMap results;

    int index = TorcHTMLServicesHelp::staticMetaObject.indexOfClassInfo("Version");
    results.insert("version",   index > -1 ? TorcHTMLServicesHelp::staticMetaObject.classInfo(index).value() : "unknown");
    results.insert("services",  GetServiceList());
    results.insert("starttime", GetStartTime());
    results.insert("priority",  GetPriority());
    results.insert("uuid",      GetUuid());

    return results;
}

QVariantMap TorcHTMLServicesHelp::GetServiceList(void)
{
    return TorcHTTPServer::GetServiceHandlers();
}

QVariantMap TorcHTMLServicesHelp::GetServiceDescription(const QString &Service)
{
    // the server has the services - and can lock the list
    return TorcHTTPServer::GetServiceDescription(Service);
}

QVariantList TorcHTMLServicesHelp::GetReturnFormats(void)
{
    return returnFormats;
}

QVariantList TorcHTMLServicesHelp::GetWebSocketProtocols(void)
{
    return TorcWebSocket::GetSupportedSubProtocols();
}

qint64 TorcHTMLServicesHelp::GetStartTime(void)
{
    return gLocalContext->GetStartTime();
}

int TorcHTMLServicesHelp::GetPriority(void)
{
    return gLocalContext->GetPriority();
}

QString TorcHTMLServicesHelp::GetUuid(void)
{
    return gLocalContext->GetUuid();
}

/*! \brief Return a WebSocket token for connecting a WebSocket when authentication is required.
 *
 * \note This is a dummy method as there is no point in calling it internally. The actual implementation
 *       is captured in ProcessHTTPRequest as it requires access to the underlying HTTP headers to authenticate.
*/
QString TorcHTMLServicesHelp::GetWebSocketToken(void)
{
    return QString("");
}

void TorcHTMLServicesHelp::HandlersChanged(void)
{
    emit ServiceListChanged();
}
