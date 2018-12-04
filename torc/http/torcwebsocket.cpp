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

// Qt
#include <QUrl>
#include <QTimer>
#include <QTextStream>
#include <QJsonDocument>
#include <QCryptographicHash>

// Torc
#include "torclogging.h"
#include "torcnetwork.h"
#include "torcnetworkedcontext.h"
#include "torchttprequest.h"
#include "torcrpcrequest.h"
#include "torchttpserver.h"
#include "torcwebsocket.h"

/*! \class TorcWebSocket
 *  \brief Overlays the Websocket protocol over a QTcpSocket
 *
 * \sa TorcHTTPServer
 * \sa TorcHTTPRequest
 * \sa TorcWebSocketThread
 *
 * TorcWebSocket is NOT thread safe. It will make direct function calls to registered services - which must
 * ensure those functions are thread safe. Likewise RemoteRequest etc can be called from any thread. For
 * thread safe operation - use TorcWebSocketThread.
 *
 * \note SubProtocol support is currently limited to JSON-RPC. New subprotocols with mixed frame support
 *       (Binary and Text) are not currently supported.
 *
 * \note To test using the Autobahn python test suite, configure the suite to
 *       request a connection using 'echo' as the method (e.g. 'http://your-ip-address:your-port/echo').
 *
 * \todo Limit frame size for reading
 * \todo Fix testsuite partial failures (fail fast on invalid UTF-8)
 * \todo Add timeout for response to upgrade request
*/

TorcWebSocket::TorcWebSocket(TorcWebSocketThread* Parent, qintptr SocketDescriptor, bool Secure)
  : QSslSocket(),
    m_parent(Parent),
    m_debug(),
    m_secure(Secure),
    m_socketState(SocketState::DisconnectedSt),
    m_socketDescriptor(SocketDescriptor),
    m_watchdogTimer(),
    m_reader(),
    m_wsReader(*this, TorcWebSocketReader::SubProtocolNone, true),
    m_authenticated(false),
    m_challengeResponse(),
    m_address(QHostAddress()),
    m_port(0),
    m_serverSide(true),
    m_subProtocol(TorcWebSocketReader::SubProtocolNone),
    m_subProtocolFrameFormat(TorcWebSocketReader::OpText),
    m_currentRequestID(1),
    m_currentRequests(),
    m_requestTimers(),
    m_subscribers()
{
    connect(&m_watchdogTimer, &QTimer::timeout, this, &TorcWebSocket::TimedOut);
    m_watchdogTimer.start(HTTP_SOCKET_TIMEOUT);
}

TorcWebSocket::TorcWebSocket(TorcWebSocketThread* Parent, const QHostAddress &Address, quint16 Port, bool Secure, TorcWebSocketReader::WSSubProtocol Protocol)
  : QSslSocket(),
    m_parent(Parent),
    m_debug(),
    m_secure(Secure),
    m_socketState(SocketState::DisconnectedSt),
    m_socketDescriptor(0),
    m_watchdogTimer(),
    m_reader(),
    m_wsReader(*this, Protocol, false),
    m_authenticated(false),
    m_challengeResponse(),
    m_address(Address),
    m_port(Port),
    m_serverSide(false),
    m_subProtocol(Protocol),
    m_subProtocolFrameFormat(TorcWebSocketReader::FormatForSubProtocol(Protocol)),
    m_currentRequestID(1),
    m_currentRequests(),
    m_requestTimers(),
    m_subscribers()
{
    // NB outgoing connection - do not start watchdog timer
}

TorcWebSocket::~TorcWebSocket()
{
    // cancel any outstanding requests
    if (!m_currentRequests.isEmpty())
    {
        // clients should be cancelling requests before closing the connection, so this
        // may represent a leak
        LOG(VB_GENERAL, LOG_WARNING, QString("%1 outstanding RPC requests").arg(m_currentRequests.size()));

        while (!m_currentRequests.isEmpty())
            CancelRequest(m_currentRequests.constBegin().value());
    }

    if (m_socketState == SocketState::Upgraded)
        m_wsReader.InitiateClose(TorcWebSocketReader::CloseGoingAway, QString("WebSocket exiting normally"));

    CloseSocket();
}

void TorcWebSocket::HandleUpgradeRequest(TorcHTTPRequest &Request)
{
    if (!m_serverSide)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to send response to upgrade request but not server side");
        SetState(SocketState::ErroredSt);
        return;
    }

    if (m_socketState != SocketState::ConnectedTo)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to send response to upgrade request but not in connected state (HTTP)");
        SetState(SocketState::ErroredSt);
        return;
    }

    bool valid = true;
    bool versionerror = false;
    QString error;
    TorcWebSocketReader::WSSubProtocol protocol = TorcWebSocketReader::SubProtocolNone;

    /* Excerpt from RFC 6455

    The requirements for this handshake are as follows.

       1.   The handshake MUST be a valid HTTP request as specified by
            [RFC2616].

       2.   The method of the request MUST be GET, and the HTTP version MUST
            be at least 1.1.

            For example, if the WebSocket URI is "ws://example.com/chat",
            the first line sent should be "GET /chat HTTP/1.1".
    */

    if (valid && (Request.GetHTTPRequestType() != HTTPGet || Request.GetHTTPProtocol() != HTTPOneDotOne))
    {
        error = "Must be GET and HTTP/1.1";
        valid = false;
    }

    /*
       3.   The "Request-URI" part of the request MUST match the /resource
            name/ defined in Section 3 (a relative URI) or be an absolute
            http/https URI that, when parsed, has a /resource name/, /host/,
            and /port/ that match the corresponding ws/wss URI.
    */

    if (valid && Request.GetPath().isEmpty())
    {
        error = "invalid Request-URI";
        valid = false;
    }

    /*
       4.   The request MUST contain a |Host| header field whose value
            contains /host/ plus optionally ":" followed by /port/ (when not
            using the default port).
    */

    if (valid && !Request.Headers().contains("Host"))
    {
        error = "No Host header";
        valid = false;
    }

    // default ports (e.g. 80) are not listed in host, so don't check in this case
    QUrl host("http://" + Request.Headers().value("Host"));
    int localport = localPort();

    if (valid && localport != 80 && localport != 443 && host.port() != localport)
    {
        error = "Invalid Host port";
        valid = false;
    }

    // disable host check. It offers us no additional security and may be a raw
    // ip address, domain name or or other host name.
#if 0
    if (valid && host.host() != localAddress().toString())
    {
        error = "Invalid Host";
        valid = false;
    }
#endif
    /*
       5.   The request MUST contain an |Upgrade| header field whose value
            MUST include the "websocket" keyword.
    */

    if (valid && !Request.Headers().contains("Upgrade"))
    {
        error = "No Upgrade header";
        valid = false;
    }

    if (valid && !Request.Headers().value("Upgrade").contains("websocket", Qt::CaseInsensitive))
    {
        error = "No 'websocket request";
        valid = false;
    }

    /*
       6.   The request MUST contain a |Connection| header field whose value
            MUST include the "Upgrade" token.
    */

    if (valid && !Request.Headers().contains("Connection"))
    {
        error = "No Connection header";
        valid = false;
    }

    if (valid && !Request.Headers().value("Connection").contains("Upgrade", Qt::CaseInsensitive))
    {
        error = "No Upgrade request";
        valid = false;
    }

    /*
       7.   The request MUST include a header field with the name
            |Sec-WebSocket-Key|.  The value of this header field MUST be a
            nonce consisting of a randomly selected 16-byte value that has
            been base64-encoded (see Section 4 of [RFC4648]).  The nonce
            MUST be selected randomly for each connection.

            NOTE: As an example, if the randomly selected value was the
            sequence of bytes 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09
            0x0a 0x0b 0x0c 0x0d 0x0e 0x0f 0x10, the value of the header
            field would be "AQIDBAUGBwgJCgsMDQ4PEC=="
    */

    if (valid && !Request.Headers().contains("Sec-WebSocket-Key"))
    {
        error = "No Sec-WebSocket-Key header";
        valid = false;
    }

    if (valid && Request.Headers().value("Sec-WebSocket-Key").isEmpty())
    {
        error = "Invalid Sec-WebSocket-Key";
        valid = false;
    }

    /*
       8.   The request MUST include a header field with the name |Origin|
            [RFC6454] if the request is coming from a browser client.  If
            the connection is from a non-browser client, the request MAY
            include this header field if the semantics of that client match
            the use-case described here for browser clients.  The value of
            this header field is the ASCII serialization of origin of the
            context in which the code establishing the connection is
            running.  See [RFC6454] for the details of how this header field
            value is constructed.

            As an example, if code downloaded from www.example.com attempts
            to establish a connection to ww2.example.com, the value of the
            header field would be "http://www.example.com".

       9.   The request MUST include a header field with the name
            |Sec-WebSocket-Version|.  The value of this header field MUST be
            13.
    */

    if (valid && !Request.Headers().contains("Sec-WebSocket-Version"))
    {
        error = "No Sec-WebSocket-Version header";
        valid = false;
    }

    int version = Request.Headers().value("Sec-WebSocket-Version").toInt();

    if (valid && version != TorcWebSocketReader::Version13 && version != TorcWebSocketReader::Version8)
    {
        error = "Unsupported WebSocket version";
        versionerror = true;
        valid = false;
    }

    /*
        10.  The request MAY include a header field with the name
        |Sec-WebSocket-Protocol|.  If present, this value indicates one
        or more comma-separated subprotocol the client wishes to speak,
        ordered by preference.  The elements that comprise this value
        MUST be non-empty strings with characters in the range U+0021 to
        U+007E not including separator characters as defined in
        [RFC2616] and MUST all be unique strings.  The ABNF for the
        value of this header field is 1#token, where the definitions of
        constructs and rules are as given in [RFC2616].
    */

    if (Request.Headers().contains("Sec-WebSocket-Protocol"))
    {
        QList<TorcWebSocketReader::WSSubProtocol> protocols = TorcWebSocketReader::SubProtocolsFromPrioritisedString(Request.Headers().value("Sec-WebSocket-Protocol"));
        if (!protocols.isEmpty())
            protocol = protocols.first();
    }

    if (!valid)
    {
        LOG(VB_GENERAL, LOG_ERR, error);
        Request.SetStatus(HTTP_BadRequest);
        if (versionerror)
            Request.SetResponseHeader("Sec-WebSocket-Version", "8,13");
        return;
    }

    // valid handshake so set response headers and transfer socket
    LOG(VB_GENERAL, LOG_DEBUG, "Received valid websocket Upgrade request");

    QString key = Request.Headers().value("Sec-WebSocket-Key") + QLatin1String("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    QString nonce = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toBase64();

    Request.SetResponseType(HTTPResponseNone);
    Request.SetStatus(HTTP_SwitchingProtocols);
    Request.SetConnection(HTTPConnectionUpgrade);
    Request.SetResponseHeader("Upgrade", "websocket");
    Request.SetResponseHeader("Sec-WebSocket-Accept", nonce);
    if (protocol != TorcWebSocketReader::SubProtocolNone)
    {
        Request.SetResponseHeader("Sec-WebSocket-Protocol", TorcWebSocketReader::SubProtocolsToString(protocol));
        m_subProtocol = protocol;
        m_subProtocolFrameFormat = TorcWebSocketReader::FormatForSubProtocol(protocol);
        m_wsReader.SetSubProtocol(protocol);
    }

    SetState(SocketState::Upgraded);
    m_authenticated = Request.IsAuthorised();

    LOG(VB_GENERAL, LOG_INFO, QString("%1 socket upgraded  (%2)").arg(m_debug, m_authenticated ? "Authenticated" : "Unauthenticated"));

    if (Request.Headers().contains("Torc-UUID"))
    {
        // stop the watchdog timer for peers
        m_watchdogTimer.stop();
        QVariantMap data;
        data.insert("uuid",       Request.Headers().value("Torc-UUID"));
        data.insert("name",       Request.Headers().value("Torc-Name"));
        data.insert("port",       Request.Headers().value("Torc-Port"));
        data.insert("priority",   Request.Headers().value("Torc-Priority"));
        data.insert("starttime",  Request.Headers().value("Torc-Starttime"));
        data.insert("apiversion", Request.Headers().value("Torc-APIVersion"));
        data.insert("address",    peerAddress().toString());
        if (Request.Headers().contains("Torc-Secure"))
            data.insert("secure", "yes");
        TorcNetworkedContext::PeerConnected(m_parent, data);
    }
    else
    {
        // increase the timeout for upgraded sockets
        m_watchdogTimer.start(FULL_SOCKET_TIMEOUT);
    }

    if (Request.GetMethod().startsWith(QStringLiteral("echo"), Qt::CaseInsensitive))
    {
        m_wsReader.EnableEcho();
        LOG(VB_GENERAL, LOG_INFO, "Enabling WebSocket echo for testing");
    }
}

///\brief Return a list of supported WebSocket sub-protocols
QVariantList TorcWebSocket::GetSupportedSubProtocols(void)
{
    QVariantList result;
    QVariantMap proto;
    proto.insert("name",TORC_JSON_RPC);
    proto.insert("description", "I can't remember how this differs from straight JSON-RPC:) The overall mechanism is very similar to WAMP.");
    result.append(proto);
    return result;
}

void TorcWebSocket::Encrypted(void)
{
    if (m_debug.isEmpty())
        m_debug = QString(">> %1 %2 -").arg(TorcNetwork::IPAddressToLiteral(peerAddress(), peerPort()), m_secure ? "secure" : "insecure");
    connect(this, &TorcWebSocket::readyRead, this, &TorcWebSocket::ReadyRead);
    emit ConnectionEstablished();
    LOG(VB_GENERAL, LOG_INFO, QString("%1 encrypted").arg(m_debug));

    if (!m_serverSide)
        Connected();
}

void TorcWebSocket::SSLErrors(const QList<QSslError> &Errors)
{
    QList<QSslError> allowed = TorcNetwork::AllowableSslErrors(Errors);
    if (!allowed.isEmpty())
        ignoreSslErrors(allowed);
}

bool TorcWebSocket::IsSecure(void)
{
    return m_secure;
}

///\brief Initialise the websocket once its parent thread is ready.
void TorcWebSocket::Start(void)
{
    connect(this, &TorcWebSocket::Disconnect, this, &TorcWebSocket::CloseSocket);
    connect(this, &TorcWebSocket::bytesWritten, this, &TorcWebSocket::BytesWritten);

    // common setup
    m_reader.Reset();
    m_wsReader.Reset();

    connect(this, static_cast<void (TorcWebSocket::*)(QAbstractSocket::SocketError)>(&TorcWebSocket::error), this, &TorcWebSocket::Error);
    connect(this, &TorcWebSocket::disconnected, this, &TorcWebSocket::Disconnected);
    connect(this, &TorcWebSocket::encrypted,    this, &TorcWebSocket::Encrypted);
    connect(this, static_cast<void (TorcWebSocket::*)(const QList<QSslError>&)>(&TorcWebSocket::sslErrors),  this, &TorcWebSocket::SSLErrors);

    // Ignore errors for Self signed certificates
    QList<QSslError> ignore;
    ignore << QSslError::SelfSignedCertificate;
    ignoreSslErrors(ignore);

    // server side:)
    if (m_serverSide && m_socketDescriptor)
    {
        if (setSocketDescriptor(m_socketDescriptor))
        {
            m_debug = QString("<< %1 %2 -").arg(TorcNetwork::IPAddressToLiteral(peerAddress(), peerPort()), m_secure ? "secure" : "insecure");
            LOG(VB_GENERAL, LOG_INFO, QString("%1 socket connected (%2)").arg(m_debug, m_authenticated ? "Authenticated" : "Unauthenticated"));
            SetState(SocketState::ConnectedTo);

            if (m_secure)
            {
                startServerEncryption();
            }
            else
            {
                connect(this, &TorcWebSocket::readyRead, this, &TorcWebSocket::ReadyRead);
                emit ConnectionEstablished();
            }
            return;
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, "Failed to set socket descriptor");
        }
    }
    else if (!m_serverSide)
    {
        SetState(SocketState::ConnectingTo);

        if (m_secure)
        {
            connectToHostEncrypted(m_address.toString(), m_port);
        }
        else
        {
            connect(this, &TorcWebSocket::connected, this, &TorcWebSocket::Connected);
            connect(this, &TorcWebSocket::readyRead, this, &TorcWebSocket::ReadyRead);
            connectToHost(m_address, m_port);
        }
        return;
    }

    // failed
    LOG(VB_GENERAL, LOG_ERR, "Failed to start Websocket");
    SetState(SocketState::ErroredSt);
}

///\brief Receives notifications when a property for a subscribed service has changed.
void TorcWebSocket::PropertyChanged(void)
{
    TorcHTTPService *service = dynamic_cast<TorcHTTPService*>(sender());
    if (service && senderSignalIndex() > -1)
    {
        TorcRPCRequest *request = new TorcRPCRequest(service->Signature() + service->GetMethod(senderSignalIndex()));
        request->AddParameter("value", service->GetProperty(senderSignalIndex()));
        m_wsReader.SendFrame(m_subProtocolFrameFormat, request->SerialiseRequest(m_subProtocol));
        request->DownRef();
    }
}

bool TorcWebSocket::HandleNotification(const QString &Method)
{
    if (m_subscribers.contains(Method))
    {
        QMap<QString,QObject*>::const_iterator it = m_subscribers.constFind(Method);
        while (it != m_subscribers.constEnd() && it.key() == Method)
        {
            QMetaObject::invokeMethod(it.value(), "ServiceNotification", Q_ARG(QString, Method));
            ++it;
        }

        return true;
    }

    return false;
}

/*! \brief Initiate a Remote Procedure Call.
 *
 * \note This should always be called from within the websocket's thread.
*/
void TorcWebSocket::RemoteRequest(TorcRPCRequest *Request)
{
    if (!Request)
        return;

    bool notification = Request->IsNotification();

    // NB notitications cannot be cancelled - they are fire and forget.
    // NB other requests cannot be cancelled before this call is processed (they will not be present in m_currentRequests)
    if (!notification)
    {
        Request->UpRef();
        int id = m_currentRequestID++;
        while (m_currentRequests.contains(id))
            id = m_currentRequestID++;

        Request->SetID(id);
        m_currentRequests.insert(id, Request);

        // start a timer for this request
        m_requestTimers.insert(startTimer(10000, Qt::CoarseTimer), id);

        // keep id's at sane values
        if (m_currentRequestID > 100000)
            m_currentRequestID = 1;
    }

    Request->AddState(TorcRPCRequest::RequestSent);

    if (m_subProtocol != TorcWebSocketReader::SubProtocolNone)
        m_wsReader.SendFrame(m_subProtocolFrameFormat, Request->SerialiseRequest(m_subProtocol));
    else
        LOG(VB_GENERAL, LOG_ERR, "No protocol specified for remote procedure call");

    // notifications are fire and forget, so downref immediately
    if (notification)
        Request->DownRef();
}

/*! \brief Cancel a Remote Procedure Call.
 *
 * \note We assume the request will only ever be referenced by its owner and by TorcWebSocket.
*/
void TorcWebSocket::CancelRequest(TorcRPCRequest *Request)
{
    if (Request && !Request->IsNotification() && Request->GetID() > -1)
    {
        Request->AddState(TorcRPCRequest::Cancelled);
        int id = Request->GetID();
        if (m_currentRequests.contains(id))
        {
            // cancel the timer
            int timer = m_requestTimers.key(id);
            killTimer(timer);
            m_requestTimers.remove(timer);

            // cancel the request
            m_currentRequests.remove(id);
            Request->DownRef();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Cannot cancel unknown RPC request");
        }

        if (Request->IsShared())
            LOG(VB_GENERAL, LOG_ERR, "RPC request is still referenced after cancellation - potential leak");
    }
}

void TorcWebSocket::ReadHTTP(void)
{
    if (!m_serverSide)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to read HTTP but not server side");
        SetState(SocketState::ErroredSt);
        return;
    }

    if (m_socketState != SocketState::ConnectedTo)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to read HTTP but not in connected state (raw HTTP)");
        SetState(SocketState::ErroredSt);
        return;
    }

    while (canReadLine())
    {
        // read data
        if (!m_reader.Read(this))
        {
            SetState(SocketState::ErroredSt);
            break;
        }

        if (!m_reader.IsReady())
            continue;

        // sanity check
        if (bytesAvailable() > 0)
            LOG(VB_GENERAL, LOG_WARNING, QString("%1 unread bytes from %2").arg(bytesAvailable()).arg(peerAddress().toString()));

        // have headers and content - process request
        TorcHTTPRequest request(&m_reader);
        request.SetSecure(m_secure);

        if (request.GetHTTPType() == HTTPResponse)
        {
            LOG(VB_GENERAL, LOG_ERR, "Received unexpected HTTP response");
            SetState(SocketState::ErroredSt);
            return;
        }

        bool upgrade = request.Headers().contains("Upgrade");
        TorcHTTPServer::Authorise(peerAddress().toString(), request, upgrade);

        if (upgrade)
        {
            HandleUpgradeRequest(request);
        }
        else
        {
            if (request.IsAuthorised() == HTTPAuthorised || request.IsAuthorised() == HTTPPreAuthorised)
            {
                TorcHTTPServer::HandleRequest(peerAddress().toString(), peerPort(),
                                              localAddress().toString(), localPort(), request);
            }
        }
        request.Respond(this);

        // reset
        m_reader.Reset();
    }
}
/*! \brief Process incoming data
 *
 * Data for any given frame may be received over a number of packets, hence the need
 * to track the current read status.
*/
void TorcWebSocket::ReadyRead(void)
{
    // restart the watchdog timer
    if (m_watchdogTimer.isActive())
        m_watchdogTimer.start();

    // Guard against spurious socket activity/connections e.g. a client trying to connect using SSL when
    // the server is not expecting secure sockets. TorcHTTPReader will be expecting a complete line but no additional
    // data is received so we loop continuously. So count retries while the available bytes remains unchanged and introduce
    // a micro sleep when available bytes does not change during the loop.
    static const int maxUnchanged   = 5000;
    static const int unchangedSleep = 1000; // microseconds (1ms) - for a total timeout of 5 seconds
    int unchangedCount = 0;

    while ((m_socketState == SocketState::ConnectedTo || m_socketState == SocketState::Upgrading || m_socketState == SocketState::Upgraded) &&
            bytesAvailable())
    {
        qint64 available = bytesAvailable();

        if (m_socketState == SocketState::ConnectedTo)
        {
            ReadHTTP();
        }

        // we may now be upgrading
        if (m_socketState == SocketState::Upgrading)
        {
            if (m_serverSide)
            {
                // the upgrade request is handled in ReadHTTP and if successfully processed we
                // move directly into the Upgraded state
                LOG(VB_GENERAL, LOG_ERR, "Upgrading state on server side");
                SetState(SocketState::ErroredSt);
                return;
            }
            else
            {
                ReadHandshake();
            }
        }

        // we may now be upgraded
        if (m_socketState == SocketState::Upgraded)
        {
            if (!m_wsReader.Read())
            {
                if (m_wsReader.CloseSent())
                    SetState(SocketState::Disconnecting);
            }
            else
            {
                // have a payload
                ProcessPayload(m_wsReader.GetPayload());
                m_wsReader.Reset();
            }
        }

        // pause if necessary
        if (bytesAvailable() == available)
        {
            unchangedCount++;
            if (unchangedCount > maxUnchanged)
            {
                LOG(VB_GENERAL, LOG_WARNING, QString("Socket time out waiting for valid data - closing"));
                SetState(SocketState::Disconnecting);
                break;
            }
            TorcQThread::usleep(unchangedSleep);
        }
    }
}

/*! \brief A subscriber object has been deleted.
 *
 * If subscribers exit without unsubscribing, the subscriber list will never be cleaned up. So listen
 * for deletion signals and act if necessary.
*/
void TorcWebSocket::SubscriberDeleted(QObject *Object)
{
    QList<QString> remove;

    QMap<QString,QObject*>::const_iterator it = m_subscribers.constBegin();
    for ( ; it != m_subscribers.constEnd(); ++it)
        if (it.value() == Object)
            remove.append(it.key());

    foreach (const QString &service, remove)
    {
        m_subscribers.remove(service, Object);
        LOG(VB_GENERAL, LOG_WARNING, QString("Removed stale subscription to '%1'").arg(service));
    }
}

void TorcWebSocket::CloseSocket(void)
{
    if(state() == QAbstractSocket::ConnectedState)
        LOG(VB_GENERAL, LOG_INFO, QString("%1 disconnecting").arg(m_debug));

    disconnectFromHost();
    // we only check for connected state - we don't care if it is any in any prior or subsequent state (hostlookup, connecting) and
    // the wait is only a 'courtesy' anyway.
    if (state() == QAbstractSocket::ConnectedState && !waitForDisconnected(1000))
        LOG(VB_GENERAL, LOG_WARNING, "WebSocket not successfully disconnected before closing");
    close();
}

void TorcWebSocket::Connected(void)
{
    m_debug = QString(">> %1 %2 -").arg(TorcNetwork::IPAddressToLiteral(peerAddress(), peerPort()), m_secure ? "secure" : "insecure");
    LOG(VB_GENERAL, LOG_INFO, QString("%1 connection to remote host").arg(m_debug));
    SetState(SocketState::Upgrading);
    SendHandshake();
}

void TorcWebSocket::SendHandshake(void)
{
    if (m_serverSide)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to send handshake from server side");
        SetState(SocketState::ErroredSt);
        return;
    }

    if (m_socketState != SocketState::Upgrading)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to upgrade from incorrect state (client side)");
        SetState(SocketState::ErroredSt);
        return;
    }

    QScopedPointer<QByteArray> upgrade(new QByteArray());
    QTextStream stream(upgrade.data());

    QByteArray nonce;
    for (int i = 0; i < 16; ++i)
        nonce.append(qrand() % 0x100);
    nonce = nonce.toBase64();

    // store the expected response for later comparison
    QString key = QString(nonce.data()) + QLatin1String("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    m_challengeResponse = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toBase64();

    QHostAddress host(m_address);
    TorcHTTPServer::Status server = TorcHTTPServer::GetStatus();

    stream << "GET / HTTP/1.1\r\n";
    stream << "User-Agent: " << TorcHTTPServer::PlatformName() << "\r\n";
    stream << "Host: " << TorcNetwork::IPAddressToLiteral(host, m_port) << "\r\n";
    stream << "Upgrade: WebSocket\r\n";
    stream << "Connection: Upgrade\r\n";
    stream << "Sec-WebSocket-Version: 13\r\n";
    stream << "Sec-WebSocket-Key: " << nonce.data() << "\r\n";
    if (m_subProtocol != TorcWebSocketReader::SubProtocolNone)
        stream << "Sec-WebSocket-Protocol: " << TorcWebSocketReader::SubProtocolsToString(m_subProtocol) << "\r\n";
    stream << "Torc-UUID: " << gLocalContext->GetUuid() << "\r\n";
    stream << "Torc-Port: " << QString::number(server.port) << "\r\n";
    stream << "Torc-Name: " << TorcHTTPServer::ServerDescription() << "\r\n";
    stream << "Torc-Priority:" << gLocalContext->GetPriority() << "\r\n";
    stream << "Torc-Starttime:" << gLocalContext->GetStartTime() << "\r\n";
    stream << "Torc-APIVersion:" << TorcHTTPServices::GetVersion() << "\r\n";
    if (server.secure)
        stream << "Torc-Secure: yes\r\n";
    stream << "\r\n";
    stream.flush();

    if (write(upgrade->data(), upgrade->size()) != upgrade->size())
    {
        LOG(VB_GENERAL, LOG_ERR, "Unexpected write error");
        SetState(SocketState::ErroredSt);
        return;
    }

    LOG(VB_GENERAL, LOG_DEBUG, QString("%1 client WebSocket connected (SubProtocol: %2)")
        .arg(m_debug, TorcWebSocketReader::SubProtocolsToString(m_subProtocol)));

    LOG(VB_NETWORK, LOG_DEBUG, QString("Data...\r\n%1").arg(upgrade->data()));
}

void TorcWebSocket::ReadHandshake(void)
{
    if (m_serverSide)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to read handshake server side");
        SetState(SocketState::ErroredSt);
        return;
    }

    if (m_socketState != SocketState::Upgrading)
    {
        LOG(VB_GENERAL, LOG_ERR, "Trying to read handshake but not upgrading");
        SetState(SocketState::ErroredSt);
        return;
    }

    // read response (which is the only raw HTTP we should see)
    if (!m_reader.Read(this))
    {
        LOG(VB_GENERAL, LOG_ERR, "Error reading upgrade response");
        SetState(SocketState::ErroredSt);
        return;
    }

    // response complete
    if (!m_reader.IsReady())
        return;

    // parse the response
    TorcHTTPRequest request(&m_reader);

    bool valid = true;
    QString error;

    // is it a response
    if (valid && request.GetHTTPType() != HTTPResponse)
    {
        valid = false;
        error = QString("Response is not an HTTP response");
    }

    // is it switching protocols
    if (valid && request.GetHTTPStatus() != HTTP_SwitchingProtocols)
    {
        valid = false;
        error = QString("Expected '%1' - got '%2'").arg(TorcHTTPRequest::StatusToString(HTTP_SwitchingProtocols),
                                                        TorcHTTPRequest::StatusToString(request.GetHTTPStatus()));
    }

    // does it contain the correct headers
    if (valid && !(request.Headers().contains("Upgrade") && request.Headers().contains("Connection") &&
                   request.Headers().contains("Sec-WebSocket-Accept")))
    {
        valid = false;
        error = QString("Response is missing required headers");
    }

    // correct header contents
    if (valid)
    {
        QString upgrade    = request.Headers().value("Upgrade").trimmed();
        QString connection = request.Headers().value("Connection").trimmed();
        QString accept     = request.Headers().value("Sec-WebSocket-Accept").trimmed();
        QString protocols  = request.Headers().value("Sec-WebSocket-Protocol").trimmed();

        if (!upgrade.contains("websocket", Qt::CaseInsensitive) || !connection.contains("upgrade", Qt::CaseInsensitive))
        {
            valid = false;
            error = QString("Unexpected header content");
        }
        else
        {
            if (!accept.contains(m_challengeResponse, Qt::CaseSensitive))
            {
                valid = false;
                error = QString("Incorrect Sec-WebSocket-Accept response");
            }
        }

        // ensure the correct subprotocol (if any) has been agreed
        if (valid)
        {
            if (m_subProtocol == TorcWebSocketReader::SubProtocolNone)
            {
                if (!protocols.isEmpty())
                {
                    valid = false;
                    error = QString("Unexpected subprotocol");
                }
            }
            else
            {
                TorcWebSocketReader::WSSubProtocols subprotocols = TorcWebSocketReader::SubProtocolsFromString(protocols);
                if ((subprotocols | m_subProtocol) != m_subProtocol)
                {
                    valid = false;
                    error = QString("Unexpected subprotocol");
                }
            }
        }
    }

    if (!valid)
    {
        LOG(VB_GENERAL, LOG_ERR, error);
        SetState(SocketState::ErroredSt);
        return;
    }

    LOG(VB_GENERAL, LOG_DEBUG, "Received valid upgrade response - switching to frame protocol");
    SetState(SocketState::Upgraded);
}

void TorcWebSocket::Error(QAbstractSocket::SocketError SocketError)
{
    if (QAbstractSocket::RemoteHostClosedError == SocketError)
        LOG(VB_GENERAL, LOG_INFO, QString("%1 remote host disconnected socket").arg(m_debug));
    else
        LOG(VB_GENERAL, LOG_ERR, QString("%1 socket error %2 '%3'").arg(m_debug).arg(SocketError).arg(errorString()));
    SetState(SocketState::ErroredSt);
}

void TorcWebSocket::TimedOut(void)
{
    if(m_socketState == SocketState::Upgraded)
        LOG(VB_GENERAL, LOG_WARNING, QString("%1 no websocket traffic for %2seconds").arg(m_debug).arg(m_watchdogTimer.interval() / 1000));
    else
        LOG(VB_GENERAL, LOG_INFO, QString("%1 no HTTP traffic for %2seconds").arg(m_debug).arg(m_watchdogTimer.interval() / 1000));
    SetState(SocketState::DisconnectedSt);
}

void TorcWebSocket::BytesWritten(qint64)
{
    if (m_watchdogTimer.isActive())
        m_watchdogTimer.start();
}

bool TorcWebSocket::event(QEvent *Event)
{
    if (Event->type() == QEvent::Timer)
    {
        QTimerEvent *event = static_cast<QTimerEvent*>(Event);
        if (event)
        {
            int timerid = event->timerId();

            if (m_requestTimers.contains(timerid))
            {
                // remove the timer
                int requestid = m_requestTimers.value(timerid);
                killTimer(timerid);
                m_requestTimers.remove(timerid);

                // handle the timeout
                TorcRPCRequest *request = nullptr;
                if (m_currentRequests.contains(requestid) && (request = m_currentRequests.value(requestid)))
                {
                    request->AddState(TorcRPCRequest::TimedOut);
                    LOG(VB_GENERAL, LOG_WARNING, QString("'%1' request timed out").arg(request->GetMethod()));

                    request->NotifyParent();

                    m_currentRequests.remove(requestid);
                    request->DownRef();
                }

                return true;
            }
        }
    }

    return QSslSocket::event(Event);
}

void TorcWebSocket::SetState(SocketState State)
{
    if (State == m_socketState)
        return;

    m_socketState = State;

    if (m_socketState == SocketState::Disconnecting || m_socketState == SocketState::DisconnectedSt ||
        m_socketState == SocketState::ErroredSt)
    {
        emit Disconnected();
    }

    if (m_socketState == SocketState::Upgraded)
        emit ConnectionUpgraded();
}

void TorcWebSocket::ProcessPayload(const QByteArray &Payload)
{
    if (m_subProtocol == TorcWebSocketReader::SubProtocolJSONRPC)
    {
        // NB there is no method to support SENDING batched requests (hence
        // we should only receive batched requests from 3rd parties) and hence there
        // is no support for handling batched responses.
        TorcRPCRequest *request = new TorcRPCRequest(m_subProtocol, Payload, this, m_authenticated);

        // if the request has data, we need to send it (it was a request!)
        if (!request->GetData().isEmpty())
        {
            m_wsReader.SendFrame(m_subProtocolFrameFormat, request->GetData());
        }
        // if the request has an id, we need to process it
        else if (request->GetID() > -1)
        {
            int id = request->GetID();
            TorcRPCRequest *requestor = nullptr;
            if (m_currentRequests.contains(id) && (requestor = m_currentRequests.value(id)))
            {
                requestor->AddState(TorcRPCRequest::ReplyReceived);

                if (request->GetState() & TorcRPCRequest::Errored)
                {
                    requestor->AddState(TorcRPCRequest::Errored);
                }
                else
                {
                    QString method = requestor->GetMethod();
                    // is this a successful response to a subscription request?
                    if (method.endsWith("/Subscribe"))
                    {
                        method.chop(9);
                        QObject *parent = requestor->GetParent();

                        if (parent->metaObject()->indexOfSlot(QMetaObject::normalizedSignature("ServiceNotification(QString)")) < 0)
                        {
                            LOG(VB_GENERAL, LOG_ERR, QString("Cannot monitor subscription to '%1' for object '%2' - no notification slot").arg(method, parent->objectName()));
                        }
                        else if (request->GetReply().type() == QVariant::Map)
                        {
                            // listen for destroyed signals to ensure the subscriptions are cleaned up
                            connect(parent, &QObject::destroyed, this, &TorcWebSocket::SubscriberDeleted);

                            QVariantMap map = request->GetReply().toMap();
                            if (map.contains("properties") && map.value("properties").type() == QVariant::List)
                            {
                                QVariantList properties = map.value("properties").toList();

                                // add each notification/parent pair to the subscriber list
                                QVariantList::const_iterator it = properties.constBegin();
                                for ( ; it != properties.constEnd(); ++it)
                                {
                                    if (it->type() == QVariant::Map)
                                    {
                                        QVariantMap property = it->toMap();
                                        if (property.contains("notification"))
                                        {
                                            QString service = method + property.value("notification").toString();
                                            if (m_subscribers.contains(service, parent))
                                            {
                                                LOG(VB_GENERAL, LOG_WARNING, QString("Object '%1' already has subscription to '%2'").arg(parent->objectName(), service));
                                            }
                                            else
                                            {
                                                m_subscribers.insertMulti(service, parent);
                                                LOG(VB_GENERAL, LOG_INFO, QString("Object '%1' subscribed to '%2'").arg(parent->objectName(), service));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // or a successful unsubscribe?
                    else if (method.endsWith("/Unsubscribe"))
                    {
                        method.chop(11);
                        QObject *parent = requestor->GetParent();

                        // iterate over our subscriber list and remove anything that starts with method and points to parent
                        QStringList remove;

                        QMap<QString,QObject*>::const_iterator it = m_subscribers.constBegin();
                        for ( ; it != m_subscribers.constEnd(); ++it)
                            if (it.value() == parent && it.key().startsWith(method))
                                remove.append(it.key());

                        foreach (const QString &signature, remove)
                        {
                            LOG(VB_GENERAL, LOG_INFO, QString("Object '%1' unsubscribed from '%2'").arg(parent->objectName(), signature));
                            m_subscribers.remove(signature, parent);
                        }

                        // and disconnect the destroyed signal if we have no more subscriptions for this object
                        if (std::find(m_subscribers.cbegin(), m_subscribers.cend(), parent) == m_subscribers.cend())
                        {
                            // temporary logging to ensure clazy optimisation is working correctly
                            LOG(VB_GENERAL, LOG_INFO, QString("'%1' disconnect - no more subscriptions").arg(parent->objectName()));
                            (void)disconnect(parent, nullptr, this, nullptr);
                        }
                    }

                    requestor->SetReply(request->GetReply());
                }

                requestor->NotifyParent();
                m_currentRequests.remove(id);
                requestor->DownRef();
            }
        }

        request->DownRef();
    }
}
