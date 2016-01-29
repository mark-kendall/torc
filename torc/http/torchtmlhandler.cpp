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
#include <QFile>
#include <QObject>
#include <QFileInfo>

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
            Request->SetAllowed(HTTPHead | HTTPGet | HTTPPost | HTTPPut | HTTPDelete | HTTPOptions);
        else
            Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);

        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseNone);
        Request->SetResponseContent(NULL);
        return;
    }

    // allowed file?
    QString url = Request->GetUrl();
    if (m_allowedFiles.contains(url))
    {
        if (url == "/")
            url = "/index.html";

        url = m_pathToContent + url;

        // file
        QFile *file = new QFile(url);

        // sanity checks
        if (file->exists())
        {
            if ((file->permissions() & QFile::ReadOther))
            {
                if (file->size() > 0)
                {
                    QDateTime modified = QFileInfo(*file).lastModified();

                    // set cache handling before we check for modification. This ensures the modification check is
                    // performed and the correct cache headers are re-sent with any 304 Not Modified response.
                    Request->SetCache(HTTPCacheNone, modified.toString(TorcHTTPRequest::DateFormat));

                    // Unmodified will handle the response
                    if (Request->Unmodified(modified))
                    {
                        delete file;
                        return;
                    }

                    Request->SetResponseFile(file);
                    Request->SetStatus(HTTP_OK);
                    Request->SetAllowGZip(true);
                    return;
                }
            }
        }

        delete file;
    }

    Request->SetResponseType(HTTPResponseNone);
    Request->SetStatus(HTTP_NotFound);
}
