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
#include "torclocaldefs.h"
#include "torcdirectories.h"
#include "torchttprequest.h"
#include "torchtmldynamiccontent.h"

/*! \class TorcHTMLDynamicContent
 *  \brief Handles the provision of dynamic content as typically located in ~/.torc/content
*/

TorcHTMLDynamicContent::TorcHTMLDynamicContent()
  : TorcHTTPHandler(TORC_CONTENT_DIR, QStringLiteral("dynamic")),
    m_pathToContent(GetTorcConfigDir())
{
    m_recursive = true;

    // Create the content directory if needed
    // TorcLocalContext should already have created the config directory
    if (m_pathToContent.endsWith('/'))
        m_pathToContent.chop(1);
    QString configdir = GetTorcContentDir();

    QDir dir(configdir);
    if (!dir.exists())
        if (!dir.mkpath(configdir))
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create content directory ('%1')").arg(configdir));
    }

void TorcHTMLDynamicContent::ProcessHTTPRequest(const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request)
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

    // restrict access to config file - require authentication
    if (Request.GetUrl().trimmed().endsWith(TORC_CONFIG_FILE))
    {
        if (!TorcHTTPHandler::MethodIsAuthorised(Request, HTTPAuth))
            return;
    }

    // get the requested file subpath
    HandleFile(Request, m_pathToContent + Request.GetUrl(), HTTPCacheNone);
}
