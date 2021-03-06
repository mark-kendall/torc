/* Class TorcHTMLHandler
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
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
#include "torcmime.h"
#include "torchttpserver.h"
#include "torchttprequest.h"
#include "torchttpservice.h"
#include "torchtmlhandler.h"

/*! \class TorcHTMLHandler
 *  \brief Base HTML handler
 *
 * Serves up the top level HTTP directory (i.e. "/").
 *
 * As the interface is predominantly Javascript created and driven, the only HTML file is 'index.html'.
 *
 * Other files present here are 'torc.xsd' (the configuration schema description) and 'manifest.json' and 'browserconfig.xml',
 * both of which are used by clients for web app purposes. Certain clients also expect icons and web app images
 * to be located in the root directort, so any request for 'png' or 'ico' files are 'redirected' to the '/img' directory.
 *
 * All other files are blocked - purely to keep the root directory clean and enforce use of the directory structure.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
*/

TorcHTMLHandler::TorcHTMLHandler(const QString &Path, const QString &Name)
  : TorcHTTPHandler(Path, Name),
    m_pathToContent(GetTorcShareDir()),
    m_allowedFiles()
{
    if (m_pathToContent.endsWith('/'))
        m_pathToContent.chop(1);
    m_pathToContent += QStringLiteral("/html");

    m_allowedFiles << QStringLiteral("/") << QStringLiteral("/index.html") << QStringLiteral("/torc.xsd") <<
                      QStringLiteral("/browserconfig.xml") << QStringLiteral("/manifest.json");
}

void TorcHTMLHandler::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
{
    (void)PeerAddress;
    (void)PeerPort;
    (void)LocalAddress;
    (void)LocalPort;

    // handle options request
    if (Request.GetHTTPRequestType() == HTTPOptions)
    {
        // this is the 'global' options - return everything possible
        if (Request.GetUrl() == QStringLiteral("/*"))
            HandleOptions(Request, HTTPHead | HTTPGet | HTTPPost | HTTPPut | HTTPDelete | HTTPOptions);
        else
            HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        return;
    }

    // allowed file?
    QString url = Request.GetUrl();
    if (m_allowedFiles.contains(url))
    {
        if (url == QStringLiteral("/"))
            url = QStringLiteral("/index.html");

        HandleFile(Request, m_pathToContent + url, HTTPCacheNone);
    }
    // I think this is unused/unreachable. img directory is served by TorcHTMLStaticContent
    else if (url.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive) || url.endsWith(QStringLiteral(".ico"), Qt::CaseInsensitive))
    {
        HandleFile(Request, m_pathToContent + "/img" + url, HTTPCacheLongLife | HTTPCacheLastModified);
    }
    else
    {
        Request.SetResponseType(HTTPResponseNone);
        Request.SetStatus(HTTP_NotFound);
    }
}
