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

// Qt
#include <QFile>
#include <QSslKey>
#include <QSslCipher>
#include <QSslConfiguration>

// Torc
#include "torclogging.h"
#include "torcdirectories.h"
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

TorcWebSocketThread::TorcWebSocketThread(qintptr SocketDescriptor, bool Secure)
  : TorcQThread("SocketIn"),
    m_webSocket(NULL),
    m_secure(Secure),
    m_socketDescriptor(SocketDescriptor),
    m_address(QHostAddress::Null),
    m_port(0),
    m_protocol(TorcWebSocketReader::SubProtocolNone)
{
}

TorcWebSocketThread::TorcWebSocketThread(const QHostAddress &Address, quint16 Port, bool Secure, TorcWebSocketReader::WSSubProtocol Protocol)
  : TorcQThread("SocketOut"),
    m_webSocket(NULL),
    m_secure(Secure),
    m_socketDescriptor(0),
    m_address(Address),
    m_port(Port),
    m_protocol(Protocol)
{
}

TorcWebSocketThread::~TorcWebSocketThread()
{
}

void TorcWebSocketThread::Start(void)
{
    // one off SSL default configuration
    static bool SSLDefaultsSet = false;
    if (!SSLDefaultsSet && m_secure)
    {
        SSLDefaultsSet = true;
        QSslConfiguration config;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
        config.setProtocol(QSsl::TlsV1_2OrLater);
        config.setCiphers(QSslConfiguration::supportedCiphers());
#else
        config.setProtocol(QSsl::TlsV1_0);
        config.setCiphers(QSslSocket::supportedCiphers());
#endif
        QString certlocation = GetTorcConfigDir() + "/torc.cert";
        LOG(VB_GENERAL, LOG_INFO, QString("SSL: looking for cert in '%1'").arg(certlocation));
        QFile certFile(certlocation);
        certFile.open(QIODevice::ReadOnly);
        if (certFile.isOpen())
        {
            QSslCertificate certificate(&certFile, QSsl::Pem);
            if (!certificate.isNull())
            {
                config.setLocalCertificate(certificate);
                LOG(VB_GENERAL, LOG_INFO, "SSL: cert loaded");
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "SSL: error loading/reading cert file");
            }
            certFile.close();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "SSL: failed to open cert file for reading");
        }

        QString keylocation  = GetTorcConfigDir() + "/torc.key";
        LOG(VB_GENERAL, LOG_INFO, QString("SSL: looking for key in '%1'").arg(keylocation));
        QFile keyFile(keylocation);
        keyFile.open(QIODevice::ReadOnly);
        if (keyFile.isOpen())
        {
            QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
            if (!key.isNull())
            {
                config.setPrivateKey(key);
                LOG(VB_GENERAL, LOG_INFO, "SSL: key loaded");
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "SSL: error loading/reading key file");
            }
            keyFile.close();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "SSL: failed to open key file for reading");
        }
        QSslConfiguration::setDefaultConfiguration(config);
    }

    if (m_socketDescriptor)
        m_webSocket = new TorcWebSocket(this, m_socketDescriptor, m_secure);
    else
        m_webSocket = new TorcWebSocket(this, m_address, m_port, m_secure, m_protocol);

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

bool TorcWebSocketThread::IsSecure(void)
{
    return m_secure;
}

void TorcWebSocketThread::RemoteRequest(TorcRPCRequest *Request)
{
    emit RemoteRequestSignal(Request);
}

void TorcWebSocketThread::CancelRequest(TorcRPCRequest *Request)
{
    emit CancelRequestSignal(Request);
}
