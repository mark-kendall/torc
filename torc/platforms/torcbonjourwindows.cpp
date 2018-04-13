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

TorcBonjour* gBonjour = NULL;                               //!< Global TorcBonjour singleton
QMutex*      gBonjourLock = new QMutex(QMutex::Recursive);  //!< Lock around access to gBonjour


/*! \class TorcBonjourWindows
 *  \brief Dummy Bonjour implementation.
 *
 *  Bonjour is not available on windows.
 *
 * \sa TorcBonjour
*/
class TorcBonjourPriv
{
  public:
    TorcBonjourPriv()  {}
    ~TorcBonjourPriv() {}
};

TorcBonjour* TorcBonjour::Instance(void)
{
    QMutexLocker locker(gBonjourLock);
    if (gBonjour)
        return gBonjour;

    gBonjour = new TorcBonjour();
    return gBonjour;
}

void TorcBonjour::Suspend(bool Suspend)
{
    (void)Suspend;
}

void TorcBonjour::TearDown(void)
{
    QMutexLocker locker(gBonjourLock);
    delete gBonjour;
    gBonjour = NULL;
}

QByteArray TorcBonjour::MapToTxtRecord(const QMap<QByteArray, QByteArray> &Map)
{
    (void)Map;
    QByteArray result;
    return result;
}

QMap<QByteArray,QByteArray> TorcBonjour::TxtRecordToMap(const QByteArray &TxtRecord)
{
    (void)TxtRecord;
    QMap<QByteArray,QByteArray> result;
    return result;
}

TorcBonjour::TorcBonjour()
  : QObject(NULL), m_priv(new TorcBonjourPriv())
{
}

TorcBonjour::~TorcBonjour()
{
    // delete private implementation
    delete m_priv;
    m_priv = NULL;
}

quint32 TorcBonjour::Register(quint16 Port, const QByteArray &Type, const QByteArray &Name,
                              const QMap<QByteArray, QByteArray> &TxtRecords)
{
    (void)Port;
    (void)Type;
    (void)Name;
    (void)TxtRecords;
    return 0;
}

quint32 TorcBonjour::Browse(const QByteArray &Type)
{
    (void)Type;
    return 0;
}

void TorcBonjour::Deregister(quint32 Reference)
{
    (void)Reference;
}

void TorcBonjour::socketReadyRead(int Socket)
{
    (void)Socket;
}

void TorcBonjour::HostLookup(const QHostInfo &HostInfo)
{
    (void)HostInfo;
}

void TorcBonjour::SuspendPriv(bool Suspend)
{
    (void)Suspend;
}

