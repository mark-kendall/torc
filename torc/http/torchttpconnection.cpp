/* Class TorcHTTPConnection
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
#include <QCoreApplication>

// Torc
#include "torclogging.h"
#include "torchttprequest.h"
#include "torchttpconnection.h"
#include "torchttpreader.h"

/*! \class TorcHTTPConnection
 *  \brief A handler for an HTTP client connection.
 *
 * TorcHTTPConnection encapsulates a current TCP connection from an HTTP client.
 * It processes incoming data and prepares complete request data to be passed
 * to TorcHTTPRequest. When a complete and valid request has been processed, it
 * is passed back to the parent TorcHTTPServer for processing.
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPHandler
 * \sa TorcHTTPRequest
 *
 * \todo Check content length early and deal with large or unexpected content payloads
*/

TorcHTTPConnection::TorcHTTPConnection(TorcHTTPServer *Parent, qintptr SocketDescriptor, int *Abort)
  : QRunnable(),
    m_abort(Abort),
    m_server(Parent),
    m_socketDescriptor(SocketDescriptor),
    m_socket(NULL)
{
}

TorcHTTPConnection::~TorcHTTPConnection()
{
}

QTcpSocket* TorcHTTPConnection::GetSocket(void)
{
    return m_socket;
}

TorcHTTPServer* TorcHTTPConnection::GetServer(void)
{
    return m_server;
}

void TorcHTTPConnection::run(void)
{
    if (!m_socketDescriptor)
        return;

    // create socket
    m_socket = new QTcpSocket();
    if (!m_socket->setSocketDescriptor(m_socketDescriptor))
    {
        LOG(VB_GENERAL, LOG_INFO, "Failed to set socket descriptor");
        return;
    }

    // debug
    QString peeraddress  = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
    QString localaddress = m_socket->localAddress().toString() + ":" + QString::number(m_socket->localPort());

    LOG(VB_NETWORK, LOG_INFO, "New connection from " + peeraddress + " on " + localaddress);

    // iniitialise state
    TorcHTTPReader *reader  = new TorcHTTPReader();
    bool connectionupgraded = false;

    while (m_server && !(*m_abort) && m_socket->state() == QAbstractSocket::ConnectedState)
    {
        // wait for data
        int count = 0;
        while (m_socket->state() == QAbstractSocket::ConnectedState && !(*m_abort) &&
               count++ < 300 && m_socket->bytesAvailable() < 1 && !m_socket->waitForReadyRead(100))
        {
        }

        // ensure we have a complete line if still waiting for complete headers.
        // This ensures we do not loop endlessly and at full load waiting for data that
        // never arrives.
        while (!reader->HeadersComplete() &&
               m_socket->state() == QAbstractSocket::ConnectedState && !(*m_abort) &&
               count++ < 300 && !m_socket->canReadLine())
        {
            QThread::msleep(100);
        }

        // timed out
        if (count >= 300)
        {
            LOG(VB_NETWORK, LOG_INFO, "No socket activity for 30 seconds");
            break;
        }

        // read data
        if (!reader->Read(m_socket, m_abort))
            break;

        if (!reader->IsReady())
            continue;

        // sanity check
        if (m_socket->bytesAvailable() > 0)
            LOG(VB_GENERAL, LOG_WARNING, QString("%1 unread bytes from %2").arg(m_socket->bytesAvailable()).arg(peeraddress));

        // have headers and content - process request
        TorcHTTPRequest *request = new TorcHTTPRequest(reader);
        bool upgrade = request->Headers()->contains("Upgrade");
        m_server->Authorise(this, request, upgrade);

        if (request->GetHTTPType() == HTTPResponse)
        {
            LOG(VB_GENERAL, LOG_ERR, "Received unexpected HTTP response");
        }
        else if (upgrade)
        {
            // if the connection is upgraded, both request and m_socket will be transferred
            // to a new thread. DO NOT DELETE!
            // N.B. we do not check for authorisation here - an 'unauthorised' upgrade request will disallow access
            // to 'setters'.
            if (TorcWebSocket::ProcessUpgradeRequest(this, request, m_socket))
            {
                connectionupgraded = true;
                break;
            }
            else
            {
                request->Respond(m_socket, m_abort);
            }
        }
        else
        {
            if (request->IsAuthorised())
                TorcHTTPServer::HandleRequest(this, request);
            request->Respond(m_socket, m_abort);
        }

        // this will delete content and headers
        delete request;

        // reset
        reader->Reset();
    }

    // cleanup
    delete reader;

    if (!connectionupgraded)
    {
        if (*m_abort)
            LOG(VB_NETWORK, LOG_INFO, "Socket closed at server's request");

        if (m_socket->state() != QAbstractSocket::ConnectedState)
            LOG(VB_NETWORK, LOG_INFO, "Socket was disconnected by remote host");

        m_socket->disconnectFromHost();
        delete m_socket;
        m_socket = NULL;

        LOG(VB_NETWORK, LOG_INFO, QString("Connection from %1 closed").arg(peeraddress));
    }
    else
    {
        LOG(VB_NETWORK, LOG_INFO, "Exiting - connection upgrading to full thread");
    }
}
