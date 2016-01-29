/* Class TorcHTMLHandler
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-16
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
#include <QObject>

// Torc
#include "torclogging.h"
#include "torcdirectories.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlhandler.h"

/*! \class TorcHTMLHandler
 *  \brief Base HTML handler
 *
 * Serves up the top level Torc HTTP interface.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPConnection
*/

TorcHTMLHandler::TorcHTMLHandler(const QString &Path, const QString &Name)
  : TorcHTTPHandler(Path, Name)
{
    m_pathToContent = GetTorcShareDir();
    if (m_pathToContent.endsWith("/"))
        m_pathToContent.chop(1);
    m_pathToContent += "/html";

    m_allowedFiles << "/" << "/index.html" << "/torc.xsd" << "/browserconfig.xml" << "/manifest.json";
}

void TorcHTMLHandler::ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection*)
{
    if (!Request)
        return;

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        // this is the 'global' options - return everything possible
        if (Request->GetUrl() == "/*")
            HandleOptions(Request, HTTPHead | HTTPGet | HTTPPost | HTTPPut | HTTPDelete | HTTPOptions);
        else
            HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        return;
    }

    // allowed file?
    QString url = Request->GetUrl();
    if (m_allowedFiles.contains(url))
    {
        if (url == "/")
            url = "/index.html";

        HandleFile(Request, m_pathToContent + url, HTTPCacheNone);
    }
    else
    {
        Request->SetResponseType(HTTPResponseNone);
        Request->SetStatus(HTTP_NotFound);
    }
}
