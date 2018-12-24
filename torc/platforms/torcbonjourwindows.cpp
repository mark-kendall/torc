/* Class TorcBonjourWindows
*
* Copyright (C) Mark Kendall 2015-18
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

// Qt
#include <QMap>

// Torc
#include "torclogging.h"
#include "torcbonjour.h"

TorcBonjour* gBonjour = nullptr;                            //!< Global TorcBonjour singleton
QMutex*      gBonjourLock = new QMutex(QMutex::Recursive);  //!< Lock around access to gBonjour

TorcBonjour* TorcBonjour::Instance(void)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        return gBonjour;

    gBonjour = new TorcBonjour();
    return gBonjour;
}

void TorcBonjour::Suspend(bool)
{
}

void TorcBonjour::TearDown(void)
{
    QMutexLocker locker(gBonjourLock);
    delete gBonjour;
    gBonjour = nullptr;
}

QByteArray TorcBonjour::MapToTxtRecord(const QMap<QByteArray, QByteArray> &)
{
    QByteArray result;
    return result;
}

QMap<QByteArray,QByteArray> TorcBonjour::TxtRecordToMap(const QByteArray &)
{
    QMap<QByteArray,QByteArray> result;
    return result;
}

TorcBonjour::TorcBonjour()
  : m_suspended(false),
    m_serviceLock(QMutex::Recursive),
    m_services(),
    m_discoveredLock(QMutex::Recursive),
    m_discoveredServices()
{
}

TorcBonjour::~TorcBonjour()
{
}

quint32 TorcBonjour::Register(quint16, const QByteArray &, const QByteArray &,
                              const QMap<QByteArray, QByteArray> &, quint32)
{
    return 0;
}

quint32 TorcBonjour::Browse(const QByteArray &, quint32)
{
    return 0;
}

void TorcBonjour::Deregister(quint32)
{
}

void TorcBonjour::socketReadyRead(int)
{
}

void TorcBonjour::HostLookup(const QHostInfo &)
{
}

void TorcBonjour::SuspendPriv(bool)
{
}

