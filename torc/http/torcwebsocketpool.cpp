/* Class TorcWebSocket
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

    while (!m_webSockets.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, "Closing outstanding websocket");
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
        if (m_webSockets[i] == thread)
        {
            TorcWebSocketThread *socket = m_webSockets.takeAt(i);

            socket->quit();
            socket->wait();
            delete socket;
            LOG(VB_NETWORK, LOG_INFO, "WebSocket thread deleted");
            break;
        }
    }
}

void TorcWebSocketPool::HandleUpgrade(TorcHTTPRequest *Request, QTcpSocket *Socket)
{
    if (!Request || ! Socket)
        return;

    QMutexLocker locker(&m_webSocketsLock);

    TorcWebSocketThread *thread = new TorcWebSocketThread(Request, Socket);
    Socket->moveToThread(thread);
    connect(thread, SIGNAL(Finished()), this, SLOT(WebSocketClosed()));
    thread->start();
    m_webSockets.append(thread);
}
