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

// Qt
#include <QDir>

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
    // TorcLocalContext should already have created the config directory
    QString configdir = GetTorcConfigDir();
    if (configdir.endsWith("/"))
        configdir.chop(1);
    m_pathToContent = configdir;
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
        HandleOptions(Request, HTTPHead | HTTPGet | HTTPOptions);
        return;
    }

    // get the requested file subpath
    HandleFile(Request, m_pathToContent + Request->GetUrl(), HTTPCacheNone);
}
