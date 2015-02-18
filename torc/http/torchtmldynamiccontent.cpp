/* Class TorcHTMLStaticContent
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
#include <QDir>
#include <QFile>
#include <QFileInfo>

// Torc
#include "torclogging.h"
#include "torcdirectories.h"
#include "torchttprequest.h"
#include "torchtmldynamiccontent.h"

/*! \class TorcHTMLDynamicContent
 *  \brief Handles the provision of dynamic content as typically located in ~/.torc/content
*/

TorcHTMLDynamicContent::TorcHTMLDynamicContent()
  : TorcHTTPHandler(DYNAMIC_DIRECTORY, "dynamic")
{
    m_recursive = true;

    // Create the content directory if needed
    // TorcLocalContext should already have created the content directory
    QString configdir = GetTorcConfigDir();
    if (configdir.endsWith("/"))
        configdir.chop(1);
    configdir.append(DYNAMIC_DIRECTORY);

    QDir dir(configdir);
    if (!dir.exists())
        if (!dir.mkdir(configdir))
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to create content directory ('%1')").arg(configdir));
}

void TorcHTMLDynamicContent::ProcessHTTPRequest(TorcHTTPRequest *Request, TorcHTTPConnection*)
{
    if (!Request)
        return;

    // handle options request
    if (Request->GetHTTPRequestType() == HTTPOptions)
    {
        Request->SetAllowed(HTTPHead | HTTPGet | HTTPOptions);
        Request->SetStatus(HTTP_OK);
        Request->SetResponseType(HTTPResponseNone);
        return;
    }

    // get the requested file subpath
    QString subpath = Request->GetUrl();

    // get the local path to dynamic content
    QString path = GetTorcConfigDir();
    if (path.endsWith("/"))
        path.chop(1);
    path += subpath;

    // file
    QFile *file = new QFile(path);

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
                Request->SetCache(HTTPCacheLongLife | HTTPCacheLastModified, modified.toString(TorcHTTPRequest::DateFormat));

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
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("'%1' is empty - ignoring").arg(path));
                Request->SetStatus(HTTP_NotFound);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("'%1' is not readable").arg(path));
            Request->SetStatus(HTTP_Forbidden);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find '%1'").arg(path));
        Request->SetStatus(HTTP_NotFound);
    }

    Request->SetResponseType(HTTPResponseNone);
    delete file;
}
