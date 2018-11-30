/* Class TorcHTTPReader
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

// Torc
#include "torclogging.h"
#include "torchttpreader.h"

/*! \class TorcHTTPReader
 *  \brief A convenience class to read HTTP requests from a QTcpSocket
 *
 * \note Both m_content and m_headers MAY be transferred to new parents for processing. It is the new owner's
 *       responsibility to delete them.
*/
TorcHTTPReader::TorcHTTPReader()
  : m_ready(false),
    m_requestStarted(false),
    m_headersComplete(false),
    m_headersRead(0),
    m_contentLength(0),
    m_contentReceived(0),
    m_method(QString()),
    m_content(),
    m_headers()
{
}

///\brief Take ownership of the contents and headers. New owner is responsible for deleting.
void TorcHTTPReader::TakeRequest(QByteArray& Content, QMap<QString,QString>& Headers)
{
    Content   = m_content;
    Headers   = m_headers;
    m_content = QByteArray();
    m_headers = QMap<QString,QString>();
}

bool TorcHTTPReader::IsReady(void) const
{
    return m_ready;
}

QString TorcHTTPReader::GetMethod(void) const
{
    return m_method;
}

bool TorcHTTPReader::HeadersComplete(void) const
{
    return m_headersComplete;
}

///\brief Reset the read state
void TorcHTTPReader::Reset(void)
{
    m_ready           = false;
    m_requestStarted  = false;
    m_headersComplete = false;
    m_headersRead     = 0;
    m_contentLength   = 0;
    m_contentReceived = 0;
    m_method          = QString();
    m_content         = QByteArray();
    m_headers         = QMap<QString,QString>();
}

/*! \brief Read and parse data from the given socket.
 *
 * The HTTP method (e.g. GET / 200 OK), headers and content will be split out
 * for further processing.
 */
bool TorcHTTPReader::Read(QTcpSocket *Socket)
{
    if (!Socket)
        return false;

    if (Socket->state() != QAbstractSocket::ConnectedState)
        return false;

    // sanity check
    if (m_headersRead >= 200)
    {
        LOG(VB_GENERAL, LOG_ERR, "Read 200 lines of headers - aborting");
        return false;
    }

    // read headers
    if (!m_headersComplete)
    {
        // filter out invalid HTTP early
        if (!m_requestStarted && Socket->bytesAvailable() >= 7 /*OPTIONS is longest valid type*/)
        {
            QByteArray buf(7, ' ');
            (void)Socket->peek(buf.data(), 7);
            if (!buf.startsWith("HTTP"))
                if (!buf.startsWith("GET"))
                    if (!buf.startsWith("PUT"))
                        if(!buf.startsWith("POST"))
                            if(!buf.startsWith("OPTIONS"))
                                if (!buf.startsWith("HEAD"))
                                    if (!buf.startsWith("DELETE"))
                                    {
                                        LOG(VB_GENERAL, LOG_ERR, QString("Invalid HTTP start ('%1')- aborting").arg(buf.constData()));
                                        return false;
                                    }
        }

        while (Socket->canReadLine() && m_headersRead < 200)
        {
            QByteArray line = Socket->readLine().trimmed();

            // an unusually long header is likely to mean this is not a valid HTTP message
            if (line.size() > 1000)
            {
                LOG(VB_GENERAL, LOG_ERR, "Header is too long - aborting");
                return false;
            }

            if (line.isEmpty())
            {
                m_headersRead = 0;
                m_headersComplete = true;
                break;
            }

            if (!m_requestStarted)
            {
                LOG(VB_NETWORK, LOG_DEBUG, line);
                m_method = line;
            }
            else
            {
                m_headersRead++;
                int index = line.indexOf(":");

                if (index > 0)
                {
                    QByteArray key   = line.left(index).trimmed();
                    QByteArray value = line.mid(index + 1).trimmed();

                    if (key == "Content-Length")
                        m_contentLength = value.toULongLong();

                    LOG(VB_NETWORK, LOG_DEBUG, QString("%1: %2").arg(key.data()).arg(value.data()));

                    m_headers.insert(key, value);
                }
            }

            m_requestStarted = true;
        }
    }

    // loop if we need more header data
    if (!m_headersComplete)
        return true;

    // abort early if needed
    if (Socket->state() != QAbstractSocket::ConnectedState)
        return false;

    // read content
    while ((m_contentReceived < m_contentLength) && Socket->bytesAvailable() &&
           Socket->state() == QAbstractSocket::ConnectedState)
    {
        static quint64 MAX_CHUNK = 32 * 1024;
        m_content.append(Socket->read(qMax(m_contentLength - m_contentReceived, qMax(MAX_CHUNK, (quint64)Socket->bytesAvailable()))));
        m_contentReceived = m_content.size();
    }

    // loop if we need more data
    if (m_contentReceived < m_contentLength)
        return true;

    // abort early if needed
    if (Socket->state() != QAbstractSocket::ConnectedState)
        return false;

    m_ready = true;
    return true;
}
