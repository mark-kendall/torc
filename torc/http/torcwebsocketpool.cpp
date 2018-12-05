/* Class TorcWebSocketPool
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

// Torc
#include "torclogging.h"
#include "torcwebsocketpool.h"

TorcWebSocketPool::TorcWebSocketPool()
  : QObject(),
    m_webSockets(),
    m_webSocketsLock(QMutex::Recursive)
{
}

TorcWebSocketPool::~TorcWebSocketPool()
{
    CloseSockets();
}

void TorcWebSocketPool::CloseSockets(void)
{
    QMutexLocker locker(&m_webSocketsLock);
    if (!m_webSockets.isEmpty())
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Closing outstanding websockets"));
    while (!m_webSockets.isEmpty())
    {
        TorcWebSocketThread* thread = m_webSockets.takeLast();
        thread->quit();
        thread->wait();
        delete thread;
    }
}

void TorcWebSocketPool::WebSocketClosed(void)
{
    QThread *thread = static_cast<QThread*>(sender());

    QMutexLocker locker(&m_webSocketsLock);

    for (int i = 0; i < m_webSockets.size(); ++i)
    {
        if (m_webSockets.value(i) == thread)
        {
            TorcWebSocketThread *socket = m_webSockets.takeAt(i);

            socket->quit();
            socket->wait();
            delete socket;
            LOG(VB_NETWORK, LOG_INFO, QStringLiteral("WebSocket thread deleted"));
            break;
        }
    }
}

void TorcWebSocketPool::IncomingConnection(qintptr SocketDescriptor, bool Secure)
{
    QMutexLocker locker(&m_webSocketsLock);
    if (m_webSockets.size() <= MAX_SOCKET_THREADS)
    {
        TorcWebSocketThread *thread = new TorcWebSocketThread(SocketDescriptor, Secure);
        m_webSockets.append(thread);
        connect(thread, &TorcWebSocketThread::Finished, this, &TorcWebSocketPool::WebSocketClosed);
        thread->start();
    }
    else
    {
        // Not sure whether this is the polite thing to do or not!
        QTcpSocket *socket = new QTcpSocket();
        socket->setSocketDescriptor(SocketDescriptor);
        socket->close();
        delete socket;
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Ignoring incoming connection - too many connections"));
    }
}

TorcWebSocketThread* TorcWebSocketPool::TakeSocket(TorcWebSocketThread *Socket)
{
    if (!Socket)
        return nullptr;

    QMutexLocker locker(&m_webSocketsLock);
    for (int i = 0; i < m_webSockets.size(); ++i)
    {
        if (m_webSockets.value(i) == Socket)
            return m_webSockets.takeAt(i);
    }

    return nullptr;
}
