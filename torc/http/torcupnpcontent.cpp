/* Class TorcUPnPContent
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

// Qt
#include <QHostAddress>
#include <QXmlStreamWriter>

// Torc
#include "torclocalcontext.h"
#include "torchttphandler.h"
#include "torcupnp.h"
#include "torcupnpcontent.h"

TorcUPnPContent::TorcUPnPContent()
  : TorcHTTPHandler(UPNP_DIRECTORY, "upnp")
{
}

void TorcUPnPContent::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
{
    (void)PeerAddress;
    (void)PeerPort;

    // handle options request
    if (Request.GetHTTPRequestType() == HTTPOptions)
    {
        HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        return;
    }

    // handle device description
    // N.B. this does not check whether the device is actually published on this interface
    if (Request.GetMethod().toLower() == "description")
    {
        QHostAddress base(LocalAddress);
        QString url = base.protocol() == QAbstractSocket::IPv6Protocol ?
                    QString("http://[%1]:%2").arg(LocalAddress).arg(LocalPort) :
                    QString("http://%1:%2").arg(LocalAddress).arg(LocalPort);
        QByteArray *result = new QByteArray();
        QXmlStreamWriter xml(result);
        xml.writeStartDocument("1.0");
        xml.writeStartElement("root");
        xml.writeAttribute("xmlns", "urn:schemas-upnp-org:device-1-0");

        xml.writeTextElement("URLBase", url);
        xml.writeStartElement("specVersion");
        xml.writeTextElement("major", "1");
        xml.writeTextElement("minor", "0");
        xml.writeEndElement(); // specversion

        xml.writeStartElement("device");

        xml.writeTextElement("deviceType", TORC_ROOT_UPNP_DEVICE);
        xml.writeTextElement("friendlyName", "Torc");
        xml.writeTextElement("manufacturer", "Torc");
        xml.writeTextElement("modelName", "Torc v1.0");
        xml.writeTextElement("UDN", QString("uuid:%1").arg(gLocalContext->GetUuid()));

        xml.writeStartElement("iconList");
        xml.writeStartElement("icon");
        xml.writeTextElement("mimetype", "image/png");
        xml.writeTextElement("width", "36");
        xml.writeTextElement("height", "36");
        xml.writeTextElement("depth", "32");
        xml.writeTextElement("url", "img/android-icon-36x36.png");
        xml.writeEndElement(); // icon
        xml.writeEndElement(); // iconlist

        xml.writeEndElement(); // device
        xml.writeEndElement(); // root
        xml.writeEndDocument();

        Request.SetAllowGZip(true);
        Request.SetStatus(HTTP_OK);
        Request.SetResponseType(HTTPResponseXML);
        Request.SetResponseContent(result);
        return;
    }
}
