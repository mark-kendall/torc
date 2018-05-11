/* Class TorcHTTPHandler
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
#include <QFile>
#include <QFileInfo>

// Torc
#include "torclogging.h"
#include "torchttpserver.h"
#include "torchttphandler.h"

/*! \class TorcHTTPHandler
 *  \brief Base HTTP response handler class.
 *
 * A TorcHTTPHandler object will be passed TorcHTTPRequest objects to process.
 * Handlers are registered with the HTTP server using TorcHTTPServer::RegisterHandler and
 * are removed with TorcHTTPServer::DeregisterHandler. The handler's signature tells the server
 * which URL the handler is expecting to process (e.g. /events).
 *
 * Concrete subclasses must implement ProcessRequest.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPRequest
*/

TorcHTTPHandler::TorcHTTPHandler(const QString &Signature, const QString &Name)
  : m_signature(Signature),
    m_recursive(false),
    m_name(Name)
{
    if (!m_signature.endsWith("/"))
        m_signature += "/";

    TorcHTTPServer::RegisterHandler(this);
}

TorcHTTPHandler::~TorcHTTPHandler()
{
    TorcHTTPServer::DeregisterHandler(this);
}

QString TorcHTTPHandler::Signature(void) const
{
    return m_signature;
}

bool TorcHTTPHandler::GetRecursive(void) const
{
    return m_recursive;
}

QString TorcHTTPHandler::Name(void) const
{
    return m_name;
}

QVariantMap TorcHTTPHandler::ProcessRequest(const QString &Method, const QVariant &Parameters, QObject *Connection, bool Authenticated)
{
    (void)Method;
    (void)Parameters;
    (void)Connection;
    (void)Authenticated;

    return QVariantMap();
}

void TorcHTTPHandler::HandleOptions(TorcHTTPRequest &Request, int Allowed)
{
    Request.SetAllowed(Allowed);
    Request.SetStatus(HTTP_OK);
    Request.SetResponseType(HTTPResponseNone);
    Request.SetResponseContent(NULL);
}

void TorcHTTPHandler::HandleFile(TorcHTTPRequest &Request, const QString &Filename, int Cache)
{
    QFile *file = new QFile(Filename);

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
                Request.SetCache(Cache, modified.toString(TorcHTTPRequest::DateFormat));

                // Unmodified will handle the response
                if (Request.Unmodified(modified))
                {
                    delete file;
                    return;
                }

                Request.SetResponseFile(file);
                Request.SetStatus(HTTP_OK);
                Request.SetAllowGZip(true);
                return;
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, QString("'%1' is empty - ignoring").arg(Filename));
                Request.SetStatus(HTTP_NotFound);
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("'%1' is not readable").arg(Filename));
            Request.SetStatus(HTTP_Forbidden);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to find '%1'").arg(Filename));
        Request.SetStatus(HTTP_NotFound);
    }

    Request.SetResponseType(HTTPResponseNone);
    delete file;
}
