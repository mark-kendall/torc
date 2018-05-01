/* Class TorcWebSocketThread
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
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
#include "torcwebsocketthread.h"

/*! \class TorcWebSocketThread
 *  \brief Wraps a TorcQThread around a TorcWebsocket
 *
 * A simple wrapper that creates and runs a TorcWebSocket in its own thread and passes through RemoteRequest
 * and CancelRequest calls. TorcWebSocket is parent 'agnostic'.
 *
 * The two different constructors either upgrade an incoming socket to a WebSocket or create a socket and
 * connect to a remote server.
 *
 * \sa TorcWebsocket
 * \sa TorcQThread
 * \sa TorcHTTPRequest
 *
 * \todo Investigate edge case leak when RemoteRequest or CancelRequest possibly might not be delivered during socket
 *       or application shutdown. Is this a real issue? (N.B. Only applies when signals are asynchronous
 *       i.e. running inside TorcWebSocketThread).
*/

TorcWebSocketThread::TorcWebSocketThread(qintptr SocketDescriptor)
  : TorcQThread("SocketIn"),
    m_webSocket(NULL),
    m_socketDescriptor(SocketDescriptor),
    m_address(QHostAddress::Null),
    m_port(0),
    m_authenticate(false),
    m_protocol(TorcWebSocket::SubProtocolNone)
{
}

TorcWebSocketThread::TorcWebSocketThread(const QHostAddress &Address, quint16 Port, bool Authenticate, TorcWebSocket::WSSubProtocol Protocol)
  : TorcQThread("SocketOut"),
    m_webSocket(NULL),
    m_socketDescriptor(0),
    m_address(Address),
    m_port(Port),
    m_authenticate(Authenticate),
    m_protocol(Protocol)
{
}

TorcWebSocketThread::~TorcWebSocketThread()
{
}

void TorcWebSocketThread::Start(void)
{
    if (m_socketDescriptor)
        m_webSocket = new TorcWebSocket(this, m_socketDescriptor);
    else
        m_webSocket = new TorcWebSocket(this, m_address, m_port, m_authenticate, m_protocol);

    connect(m_webSocket, SIGNAL(ConnectionEstablished()), this, SIGNAL(ConnectionEstablished()));
    connect(m_webSocket, SIGNAL(Disconnected()),          this, SLOT(quit()));
    // the websocket is created in its own thread so these signals will be delivered into the correct thread.
    connect(this, SIGNAL(RemoteRequestSignal(TorcRPCRequest*)), m_webSocket, SLOT(RemoteRequest(TorcRPCRequest*)));
    connect(this, SIGNAL(CancelRequestSignal(TorcRPCRequest*)), m_webSocket, SLOT(CancelRequest(TorcRPCRequest*)));
    m_webSocket->Start();
}

void TorcWebSocketThread::Finish(void)
{
    if (m_webSocket)
        delete m_webSocket;
    m_webSocket = NULL;
}

void TorcWebSocketThread::RemoteRequest(TorcRPCRequest *Request)
{
    emit RemoteRequestSignal(Request);
}

void TorcWebSocketThread::CancelRequest(TorcRPCRequest *Request)
{
    emit CancelRequestSignal(Request);
}
