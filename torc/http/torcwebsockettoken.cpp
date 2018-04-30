/* Class TorcWebSocketToken
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

// Qt
#include <QUuid>

// Torc
#include "torclogging.h"
#include "torccoreutils.h"
#include "torcwebsockettoken.h"

///\brief A simple container for authenticaion tokens.
class WebSocketAuthentication
{
  public:
    WebSocketAuthentication()
      : m_timeStamp(0)
    {
    }
    WebSocketAuthentication(quint64 Timestamp, const QString &Host)
      : m_timeStamp(Timestamp),
        m_host(Host)
    {
    }

    quint64 m_timeStamp;
    QString m_host;
};

/*! \brief Retrieve an authentication token for the given request or validate a current token.
*/
QString TorcWebSocketToken::GetWebSocketToken(TorcHTTPConnection *Connection, TorcHTTPRequest *Request, const QString &Current)
{
    static QMutex lock(QMutex::Recursive);
    static QMap<QString,WebSocketAuthentication> tokens;

    QMutexLocker locker(&lock);

    // always expire old tokens
    quint64 tooold = TorcCoreUtils::GetMicrosecondCount() - 10000000;
    QStringList old;
    QMap<QString,WebSocketAuthentication>::iterator it = tokens.begin();
    for ( ; it != tokens.end(); ++it)
        if (it.value().m_timeStamp < tooold)
            old.append(it.key());

    foreach (QString expire, old)
        tokens.remove(expire);

    // safety net
    if (!Request || !Connection)
        return QString("ServerErRor");

    // if a current token is supplied, validate it
    if (!Current.isEmpty())
    {
        if (tokens.contains(Current))
        {
            QString host = Connection->GetSocket() ? Connection->GetSocket()->peerAddress().toString() : QString("eRROr");
            // validate the host
            if (tokens.value(Current).m_host == host)
            {
                tokens.remove(Current);
                return Current;
            }
            tokens.remove(Current);
            LOG(VB_GENERAL, LOG_ERR, "Host mismatch for websocket authentication");
        }
        return QString("ErroR");
    }

    // create new
    QString uuid = QUuid::createUuid().toString().mid(1, 36);
    QString host = Connection->GetSocket() ? Connection->GetSocket()->peerAddress().toString() : QString("ErrOR");
    tokens.insert(uuid, WebSocketAuthentication(TorcCoreUtils::GetMicrosecondCount(), host));
    return uuid;
}
