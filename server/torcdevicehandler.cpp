/* Class TorcDeviceHandler
*
* Copyright (C) Mark Kendall 2016=18
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torcdevicehandler.h"

QList<TorcDeviceHandler*> TorcDeviceHandler::gTorcDeviceHandlers;
TorcDeviceHandler* TorcDeviceHandler::gTorcDeviceHandler = nullptr;
QMutex* TorcDeviceHandler::gTorcDeviceHandlersLock = new QMutex(QMutex::Recursive);

TorcDeviceHandler::TorcDeviceHandler()
  : m_handlerLock(QReadWriteLock::Recursive),
    m_nextTorcDeviceHandler(nullptr)
{
    QMutexLocker lock(gTorcDeviceHandlersLock);
    m_nextTorcDeviceHandler = gTorcDeviceHandler;
    gTorcDeviceHandler = this;
}

TorcDeviceHandler::~TorcDeviceHandler()
{
}

TorcDeviceHandler* TorcDeviceHandler::GetNextHandler(void)
{
    return m_nextTorcDeviceHandler;
}

TorcDeviceHandler* TorcDeviceHandler::GetDeviceHandler(void)
{
    return gTorcDeviceHandler;
}

void TorcDeviceHandler::Start(const QVariantMap &Details)
{
    TorcDeviceHandler* handler = TorcDeviceHandler::GetDeviceHandler();
    for ( ; handler; handler = handler->GetNextHandler())
        handler->Create(Details);
}

void TorcDeviceHandler::Stop(void)
{
    TorcDeviceHandler* handler = TorcDeviceHandler::GetDeviceHandler();
    for ( ; handler; handler = handler->GetNextHandler())
        handler->Destroy();
}
