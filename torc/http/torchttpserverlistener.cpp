/* Class TorcHTTPServerAddress
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
#include "torcnetwork.h"
#include "torchttpserverlistener.h"

TorcHTTPServerListener::TorcHTTPServerListener(QObject *Parent, const QHostAddress &Address, int Port)
  : QTcpServer()
{
    if (!Parent)
        return;

    connect(this, SIGNAL(NewConnection(qintptr)), Parent, SLOT(NewConnection(qintptr)));

    if (!listen(Address, Port))
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to listen on %1").arg(TorcNetwork::IPAddressToLiteral(Address, Port)));
    else
        LOG(VB_GENERAL, LOG_INFO, QString("Listening on %1").arg(TorcNetwork::IPAddressToLiteral(Address, Port)));
}

TorcHTTPServerListener::~TorcHTTPServerListener()
{
    close();
}

void TorcHTTPServerListener::incomingConnection(qintptr SocketDescriptor)
{
    emit NewConnection(SocketDescriptor);
}
