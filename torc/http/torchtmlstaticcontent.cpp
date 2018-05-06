/* Class TorcHTMLStaticContent
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013-18
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
#include "torclanguage.h"
#include "torcdirectories.h"
#include "torcnetwork.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlstaticcontent.h"

/*! \class TorcHTMLStaticContent
 *  \brief Handles the provision of static server content such as html, css, js etc
*/

TorcHTMLStaticContent::TorcHTMLStaticContent()
  : TorcHTTPHandler(STATIC_DIRECTORY, "static"),
    m_pathToContent("")
{
    m_pathToContent = GetTorcShareDir();
    if (m_pathToContent.endsWith("/"))
        m_pathToContent.chop(1);
    m_pathToContent += "/html";

    m_recursive = true;
}

void TorcHTMLStaticContent::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
{
    (void)PeerAddress;
    (void)PeerPort;
    (void)LocalAddress;
    (void)LocalPort;

    // handle options request
    if (Request.GetHTTPRequestType() == HTTPOptions)
    {
        HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        return;
    }

    // get the requested file subpath
    QString subpath = Request.GetUrl();

    // request for Torc configuration. This is used to add some appropriate Javascript globals
    // and translated strings

    if (subpath == "/js/torcconfiguration.js")
    {
        GetJavascriptConfiguration(Request);
        Request.SetAllowGZip(true);
        return;
    }

    HandleFile(Request, m_pathToContent + subpath, HTTPCacheLongLife | HTTPCacheLastModified);
}

/*! \brief Construct a Javascript object that encapsulates Torc variables, enumerations and translated strings.
 *
 * The contents of this object are available as 'torcconfiguration.js'.
 *
 * \todo Translations are static and will need to be regenerated if the language is changed.
*/
void TorcHTMLStaticContent::GetJavascriptConfiguration(TorcHTTPRequest &Request)
{
    // populate the list of static constants and translations
    QVariantMap strings = TorcStringFactory::GetTorcStrings();

    // generate dynamic variables
    strings.insert("ServicesPath", SERVICES_DIRECTORY);

    // and generate javascript
    QJsonObject json = QJsonObject::fromVariantMap(strings);
    QByteArray *result = new QByteArray("var torc = ");
    result->append(QJsonDocument(json).toJson());
    if (result->endsWith("\n"))
        result->chop(1);
    result->append(";\r\n\r\nif (Object.freeze) { Object.freeze(torc); }\r\n");

    Request.SetStatus(HTTP_OK);
    Request.SetResponseType(HTTPResponseJSONJavascript);
    Request.SetResponseContent(result);
}
