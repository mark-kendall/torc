/* Class TorcWebSocketReader
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
#include <QtEndian>

// Torc
#include "torclogging.h"
#include "torcwebsocketreader.h"

// utf8
#include "utf8/core.h"
#include "utf8/checked.h"
#include "utf8/unchecked.h"

///\brief Convert OpCode to human readable string
QString TorcWebSocketReader::OpCodeToString(OpCode Code)
{
    switch (Code)
    {
        case OpContinuation: return QString("Continuation");
        case OpText:         return QString("Text");
        case OpBinary:       return QString("Binary");
        case OpClose:        return QString("Close");
        case OpPing:         return QString("Ping");
        case OpPong:         return QString("Pong");
        default: break;
    }

    return QString("Reserved");
}

///\brief Convert CloseCode to human readable string
QString TorcWebSocketReader::CloseCodeToString(CloseCode Code)
{
    switch (Code)
    {
        case CloseNormal:              return QString("Normal");
        case CloseGoingAway:           return QString("GoingAway");
        case CloseProtocolError:       return QString("ProtocolError");
        case CloseUnsupportedDataType: return QString("UnsupportedDataType");
        case CloseReserved1004:        return QString("Reserved");
        case CloseStatusCodeMissing:   return QString("StatusCodeMissing");
        case CloseAbnormal:            return QString("Abnormal");
        case CloseInconsistentData:    return QString("InconsistentData");
        case ClosePolicyViolation:     return QString("PolicyViolation");
        case CloseMessageTooBig:       return QString("MessageTooBig");
        case CloseMissingExtension:    return QString("MissingExtension");
        case CloseUnexpectedError:     return QString("UnexpectedError");
        case CloseTLSHandshakeError:   return QString("TLSHandshakeError");
        default: break;
    }

    return QString("Unknown");
}

TorcWebSocketReader::OpCode TorcWebSocketReader::FormatForSubProtocol(WSSubProtocol Protocol)
{
    switch (Protocol)
    {
        case TorcWebSocketReader::SubProtocolNone:
            return OpText;
        case TorcWebSocketReader::SubProtocolJSONRPC:
            return OpText;
        default: break;
    }

    return OpText;
}

///\brief Convert SubProtocols to HTTP readable string.
QString TorcWebSocketReader::SubProtocolsToString(WSSubProtocols Protocols)
{
    QStringList list;

    if (Protocols.testFlag(SubProtocolJSONRPC)) list.append(TORC_JSON_RPC.toLatin1());

    return list.join(",");
}

///\brief Parse supported WSSubProtocols from Protocols.
TorcWebSocketReader::WSSubProtocols TorcWebSocketReader::SubProtocolsFromString(const QString &Protocols)
{
    WSSubProtocols protocols = SubProtocolNone;

    if (Protocols.contains(TORC_JSON_RPC.toLatin1(), Qt::CaseInsensitive)) protocols |= SubProtocolJSONRPC;

    return protocols;
}

///\brief Parse a prioritised list of supported WebSocket sub-protocols.
QList<TorcWebSocketReader::WSSubProtocol> TorcWebSocketReader::SubProtocolsFromPrioritisedString(const QString &Protocols)
{
    QList<WSSubProtocol> results;
    results.reserve(1);
    QStringList protocols = Protocols.split(",");
    for (int i = 0; i < protocols.size(); ++i)
        if (!QString::compare(protocols[i].trimmed(), TORC_JSON_RPC.toLatin1(), Qt::CaseInsensitive))
            results.append(SubProtocolJSONRPC);
    return results;
}
TorcWebSocketReader::TorcWebSocketReader(QTcpSocket &Socket, WSSubProtocol Protocol, bool ServerSide)
  : m_socket(Socket),
    m_serverSide(ServerSide),
    m_closeSent(false),
    m_closeReceived(false),
    m_echoTest(false),
    m_subProtocol(Protocol),
    m_subProtocolFrameFormat(FormatForSubProtocol(Protocol)),
    m_readState(ReadHeader),
    m_frameOpCode(OpContinuation),
    m_frameFinalFragment(false),
    m_frameMasked(false),
    m_haveBufferedPayload(false),
    m_bufferedPayload(),
    m_bufferedPayloadOpCode(OpContinuation),
    m_framePayloadLength(0),
    m_framePayloadReadPosition(0),
    m_frameMask(QByteArray(4, 0)),
    m_framePayload(QByteArray())
{
}

const QByteArray& TorcWebSocketReader::GetPayload(void)
{
    return m_haveBufferedPayload ? m_bufferedPayload : m_framePayload;
}

void TorcWebSocketReader::Reset(void)
{
    m_haveBufferedPayload      = false;
    m_bufferedPayload          = QByteArray();
    m_readState                = ReadHeader;
    m_framePayload             = QByteArray();
    m_framePayloadReadPosition = 0;
    m_framePayloadLength       = 0;
}

bool TorcWebSocketReader::CloseSent(void)
{
    return m_closeSent;
}

void TorcWebSocketReader::EnableEcho(void)
{
    m_echoTest = true;
}

void TorcWebSocketReader::SetSubProtocol(WSSubProtocol Protocol)
{
    m_subProtocol = Protocol;
    m_subProtocolFrameFormat = FormatForSubProtocol(Protocol);
}

void TorcWebSocketReader::InitiateClose(CloseCode Close, const QString &Reason)
{
    if (!m_closeSent)
    {
        QByteArray payload;
        payload.append((Close >> 8) & 0xff);
        payload.append(Close & 0xff);
        payload.append(Reason.toUtf8());
        SendFrame(OpClose, payload);
        m_closeSent = true;
    }
}

void TorcWebSocketReader::SendFrame(OpCode Code, QByteArray &Payload)
{
    // don't send if OpClose has already been sent or OpClose received and
    // we're sending anything other than the echoed OpClose
    if (m_closeSent || (m_closeReceived && Code != OpClose))
        return;

    QByteArray frame;
    QByteArray mask;
    QByteArray size;

    // no fragmentation yet - so this is always the final fragment
    frame.append(Code | 0x80);

    quint8 byte = 0;

    // is this masked
    if (!m_serverSide)
    {
        for (int i = 0; i < 4; ++i)
            mask.append(qrand() % 0x100);

        byte |= 0x80;
    }

    // generate correct size
    quint64 length = Payload.size();

    if (length < 126)
    {
        byte |= length;
    }
    else if (length <= 0xffff)
    {
        byte |= 126;
        size.append((length >> 8) & 0xff);
        size.append(length & 0xff);
    }
    else if (length <= 0x7fffffff)
    {
        byte |= 127;
        size.append((length >> 56) & 0xff);
        size.append((length >> 48) & 0xff);
        size.append((length >> 40) & 0xff);
        size.append((length >> 32) & 0xff);
        size.append((length >> 24) & 0xff);
        size.append((length >> 16) & 0xff);
        size.append((length >> 8 ) & 0xff);
        size.append((length      ) & 0xff);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Infeasibly large payload!");
        return;
    }

    frame.append(byte);
    if (!size.isEmpty())
        frame.append(size);

    if (!m_serverSide)
    {
        frame.append(mask);
        for (int i = 0; i < Payload.size(); ++i)
            Payload[i] = Payload[i] ^ mask[i % 4];
    }

    if (m_socket.write(frame) == frame.size())
    {
        if ((!Payload.isEmpty() && m_socket.write(Payload) == Payload.size()) || Payload.isEmpty())
        {
            LOG(VB_NETWORK, LOG_DEBUG, QString("Sent frame (Final), OpCode: '%1' Masked: %2 Length: %3")
                .arg(OpCodeToString(Code)).arg(!m_serverSide).arg(Payload.size()));
            return;
        }
    }

    if (Code != OpClose)
        InitiateClose(CloseUnexpectedError, QString("Send error"));
}

void TorcWebSocketReader::HandlePing(QByteArray &Payload)
{
    // ignore pings once in close down
    if (m_closeReceived || m_closeSent)
        return;

    SendFrame(OpPong, Payload);
}

void TorcWebSocketReader::HandlePong(QByteArray &Payload)
{
    // TODO validate known pings
    (void)Payload;
}

/*! \brief Compose and send a properly formatted websocket frame.
 *
 * \note If this is a client side socket, Payload will be masked in place.
*/
void TorcWebSocketReader::HandleCloseRequest(QByteArray &Close)
{
    CloseCode newclosecode = CloseNormal;
    int closecode = CloseNormal;
    QString reason;

    if (Close.size() < 1)
    {
        QByteArray newpayload;
        newpayload.append((closecode >> 8) & 0xff);
        newpayload.append(closecode & 0xff);
        Close = newpayload;
    }
    // payload size 1 is invalid
    else if (Close.size() == 1)
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid close payload size (<2)");
        newclosecode = CloseProtocolError;
    }
    // check close code if present
    else if (Close.size() > 1)
    {
        closecode = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(Close.data()));

        if (closecode < CloseNormal || closecode > 4999 || closecode == CloseReserved1004 ||
            closecode == CloseStatusCodeMissing || closecode == CloseAbnormal || closecode == CloseTLSHandshakeError)
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid close code");
            newclosecode = CloseProtocolError;
        }
    }

    // check close reason if present
    if (Close.size() > 2 && newclosecode == CloseNormal)
    {
        if (!utf8::is_valid(Close.data() + 2, Close.data() + Close.size()))
        {
            LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8 in close payload");
            newclosecode = CloseInconsistentData;
        }
        else
        {
            reason = QString::fromUtf8(Close.data() + 2, Close.size() -2);
        }
    }

    if (newclosecode != CloseNormal)
    {
        QByteArray newpayload;
        newpayload.append((newclosecode >> 8) & 0xff);
        newpayload.append(newclosecode & 0xff);
        Close = newpayload;
    }
    else
    {
        LOG(VB_NETWORK, LOG_INFO, QString("Received Close: %1 ('%2')").arg(CloseCodeToString((CloseCode)closecode), reason));
    }

    m_closeReceived = true;

    if (!m_closeSent)
    {
        // echo back the payload and close request
        SendFrame(OpClose, Close);
        m_closeSent = true;
    }
}

bool TorcWebSocketReader::Read(void)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return false;

    // I'm not sure what this was trying to do in the old TorcWebSocket::ReadyRead code
    // || (m_readState == ReadPayload && m_framePayloadLength == 0)
    switch (m_readState)
    {
        case ReadHeader:
        {
            // we need at least 2 bytes to start reading
            if (m_socket.bytesAvailable() < 2)
                return false;

            char header[2];
            if (m_socket.read(header, 2) != 2)
            {
                InitiateClose(CloseUnexpectedError, QString("Read error"));
                return false;
            }

            m_frameFinalFragment = (header[0] & 0x80) != 0;
            m_frameOpCode        = static_cast<OpCode>(header[0] & 0x0F);
            m_frameMasked        = (header[1] & 0x80) != 0;
            quint8 length        = (header[1] & 0x7F);
            bool reservedbits    = (header[0] & 0x70) != 0;

            // validate the header against current state and specification
            CloseCode error = CloseNormal;
            QString reason;

            // invalid use of reserved bits
            if (reservedbits)
            {
                reason = QString("Invalid use of reserved bits");
                error = CloseProtocolError;
            }

            // control frames can only have payloads of up to 125 bytes
            else if ((m_frameOpCode & 0x8) && length > 125)
            {
                reason = QString("Control frame payload too large");
                error = CloseProtocolError;

                // need to acknowledge when an OpClose is received
                if (OpClose == m_frameOpCode)
                    m_closeReceived = true;
            }

            // if this is a server, clients must be sending masked frames and vice versa
            else if (m_serverSide != m_frameMasked)
            {
                reason = QString("Masking error");
                error = CloseProtocolError;
            }

            // we should never receive a reserved opcode
            else if (m_frameOpCode != OpText && m_frameOpCode != OpBinary && m_frameOpCode != OpContinuation &&
                m_frameOpCode != OpPing && m_frameOpCode != OpPong   && m_frameOpCode != OpClose)
            {
                reason = QString("Invalid use of reserved opcode");
                error = CloseProtocolError;
            }

            // control frames cannot be fragmented
            else if (!m_frameFinalFragment && (m_frameOpCode == OpPing || m_frameOpCode == OpPong || m_frameOpCode == OpClose))
            {
                reason = QString("Fragmented control frame");
                error = CloseProtocolError;
            }

            // a continuation frame must have an opening frame
            else if (!m_haveBufferedPayload && m_frameOpCode == OpContinuation)
            {
                reason = QString("Fragmentation error");
                error = CloseProtocolError;
            }

            // only continuation frames or control frames are expected once in the middle of a frame
            else if (m_haveBufferedPayload && !(m_frameOpCode == OpContinuation || m_frameOpCode == OpPing || m_frameOpCode == OpPong || m_frameOpCode == OpClose))
            {
                reason = QString("Fragmentation error");
                error = CloseProtocolError;
            }

            // ensure OpCode matches that expected by SubProtocol
            else if ((!m_echoTest && m_subProtocol != SubProtocolNone) &&
                     (m_frameOpCode == OpText || m_frameOpCode == OpBinary) &&
                      m_frameOpCode != m_subProtocolFrameFormat)
            {
                reason = QString("Received incorrect frame type for subprotocol");
                error  = CloseUnsupportedDataType;
            }

            if (error != CloseNormal)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Read error: %1 (%2)").arg(CloseCodeToString(error), reason));
                InitiateClose(error, reason);
                return false;
            }

            if (126 == length)
            {
                m_readState = Read16BitLength;
                break;
            }

            if (127 == length)
            {
                m_readState = Read64BitLength;
                break;
            }

            m_framePayloadLength = length;
            m_readState          = m_frameMasked ? ReadMask : ReadPayload;
        }
        break;

        case Read16BitLength:
        {
            if (m_socket.bytesAvailable() < 2)
                return false;

            uchar length[2];

            if (m_socket.read(reinterpret_cast<char *>(length), 2) != 2)
            {
                InitiateClose(CloseUnexpectedError, QString("Read error"));
                return false;
            }

            m_framePayloadLength = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(length));
            m_readState          = m_frameMasked ? ReadMask : ReadPayload;
        }
        break;

        case Read64BitLength:
        {
            if (m_socket.bytesAvailable() < 8)
                return false;

            uchar length[8];
            if (m_socket.read(reinterpret_cast<char *>(length), 8) != 8)
            {
                InitiateClose(CloseUnexpectedError, QString("Read error"));
                return false;
            }

            m_framePayloadLength = qFromBigEndian<quint64>(length) & ~(1LL << 63);
            m_readState          = m_frameMasked ? ReadMask : ReadPayload;
        }
        break;

        case ReadMask:
        {
            if (m_socket.bytesAvailable() < 4)
                return false;

            if (m_socket.read(m_frameMask.data(), 4) != 4)
            {
                InitiateClose(CloseUnexpectedError, QString("Read error"));
                return false;
            }

            m_readState = ReadPayload;
        }
        break;

        case ReadPayload:
        {
            // allocate the payload buffer if needed
            if (m_framePayloadReadPosition == 0)
                m_framePayload = QByteArray(m_framePayloadLength, 0);

            qint64 needed = m_framePayloadLength - m_framePayloadReadPosition;

            // payload length may be zero
            if (needed > 0)
            {
                qint64 red = qMin(m_socket.bytesAvailable(), needed);

                if (m_socket.read(m_framePayload.data() + m_framePayloadReadPosition, red) != red)
                {
                    InitiateClose(CloseUnexpectedError, QString("Read error"));
                    return false;
                }

                m_framePayloadReadPosition += red;
            }

            // finished
            if (m_framePayloadReadPosition == m_framePayloadLength)
            {
                LOG(VB_NETWORK, LOG_DEBUG, QString("Frame, Final: %1 OpCode: '%2' Masked: %3 Length: %4")
                    .arg(m_frameFinalFragment).arg(OpCodeToString(m_frameOpCode)).arg(m_frameMasked).arg(m_framePayloadLength));

                // unmask payload
                if (m_frameMasked)
                    for (int i = 0; i < m_framePayload.size(); ++i)
                        m_framePayload[i] = (m_framePayload[i] ^ m_frameMask[i % 4]);

                // start buffering fragmented payloads
                if (!m_frameFinalFragment && (m_frameOpCode == OpText || m_frameOpCode == OpBinary))
                {
                    // header checks should prevent this
                    if (m_haveBufferedPayload)
                    {
                        m_bufferedPayload = QByteArray();
                        LOG(VB_GENERAL, LOG_WARNING, "Already have payload buffer - clearing");
                    }

                    m_haveBufferedPayload = true;
                    m_bufferedPayload = QByteArray(m_framePayload);
                    m_bufferedPayloadOpCode = m_frameOpCode;
                }
                else if (m_frameOpCode == OpContinuation)
                {
                    if (m_haveBufferedPayload)
                        m_bufferedPayload.append(m_framePayload);
                    else
                        LOG(VB_GENERAL, LOG_ERR, "Continuation frame but no buffered payload");
                }

                // finished
                if (m_frameFinalFragment)
                {
                    if (m_frameOpCode == OpPong)
                    {
                        HandlePong(m_framePayload);
                    }
                    else if (m_frameOpCode == OpPing)
                    {
                        HandlePing(m_framePayload);
                    }
                    else if (m_frameOpCode == OpClose)
                    {
                        HandleCloseRequest(m_framePayload);
                    }
                    else
                    {
                        bool invalidtext = false;

                        // validate and debug UTF8 text
                        if (!m_haveBufferedPayload && m_frameOpCode == OpText && !m_framePayload.isEmpty())
                        {
                            if (!utf8::is_valid(m_framePayload.data(), m_framePayload.data() + m_framePayload.size()))
                            {
                                LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8");
                                invalidtext = true;
                            }
                            else
                            {
                                LOG(VB_NETWORK, LOG_DEBUG, QString("'%1'").arg(QString::fromUtf8(m_framePayload)));
                            }
                        }
                        else if (m_haveBufferedPayload && m_bufferedPayloadOpCode == OpText)
                        {
                            if (!utf8::is_valid(m_bufferedPayload.data(), m_bufferedPayload.data() + m_bufferedPayload.size()))
                            {
                                LOG(VB_GENERAL, LOG_ERR, "Invalid UTF8");
                                invalidtext = true;
                            }
                            else
                            {
                                LOG(VB_NETWORK, LOG_DEBUG, QString("'%1'").arg(QString::fromUtf8(m_bufferedPayload)));
                            }
                        }

                        if (invalidtext)
                        {
                            InitiateClose(CloseInconsistentData, "Invalid UTF-8 text");
                            return false;
                        }
                        else
                        {
                            // echo test for AutoBahn test suite
                            if (m_echoTest)
                            {
                                SendFrame(m_haveBufferedPayload ? m_bufferedPayloadOpCode : m_frameOpCode,
                                          m_haveBufferedPayload ? m_bufferedPayload : m_framePayload);
                            }
                            else
                            {
                                // we only return true when the parent needs to handle the payload
                                // and MUST then call reset()
                                return true;
                            }
                        }
                        m_haveBufferedPayload = false;
                        m_bufferedPayload = QByteArray();
                    }
                }

                // reset frame and readstate
                m_readState = ReadHeader;
                m_framePayload = QByteArray();
                m_framePayloadReadPosition = 0;
                m_framePayloadLength = 0;
            }
            else if (m_framePayload.size() > (qint64)m_framePayloadLength)
            {
                // this shouldn't happen
                InitiateClose(CloseUnexpectedError, QString("Read error"));
                return false;
            }
        }
        break;
    }

    return false;
}
