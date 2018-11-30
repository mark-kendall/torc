#ifndef TORCWEBSOCKETREADER_H
#define TORCWEBSOCKETREADER_H

// Qt
#include <QTcpSocket>

// Torc
#define TORC_JSON_RPC QString("torc.json-rpc")

class TorcWebSocketReader
{
    friend class TorcWebSocket;

  public:
    enum OpCode
    {
        OpContinuation = 0x0,
        OpText         = 0x1,
        OpBinary       = 0x2,
        OpReserved3    = 0x3,
        OpReserved4    = 0x4,
        OpReserved5    = 0x5,
        OpReserved6    = 0x6,
        OpReserved7    = 0x7,
        OpClose        = 0x8,
        OpPing         = 0x9,
        OpPong         = 0xA,
        OpReservedB    = 0xB,
        OpReservedC    = 0xC,
        OpReservedD    = 0xD,
        OpReservedE    = 0xE,
        OpReservedF    = 0xF
    };

    enum CloseCode
    {
        CloseNormal              = 1000,
        CloseGoingAway           = 1001,
        CloseProtocolError       = 1002,
        CloseUnsupportedDataType = 1003,
        CloseReserved1004        = 1004,
        CloseStatusCodeMissing   = 1005,
        CloseAbnormal            = 1006,
        CloseInconsistentData    = 1007,
        ClosePolicyViolation     = 1008,
        CloseMessageTooBig       = 1009,
        CloseMissingExtension    = 1010,
        CloseUnexpectedError     = 1011,
        CloseTLSHandshakeError   = 1015
    };

    enum WSVersion
    {
        VersionUnknown = -1,
        Version0       = 0,
        Version4       = 4,
        Version5       = 5,
        Version6       = 6,
        Version7       = 7,
        Version8       = 8,
        Version13      = 13
    };

    enum WSSubProtocol
    {
        SubProtocolNone           = (0 << 0),
        SubProtocolJSONRPC        = (1 << 0)
    };
    Q_FLAGS(WSSubProtocol)
    Q_DECLARE_FLAGS(WSSubProtocols, WSSubProtocol)

  protected:
    static QString              OpCodeToString                    (OpCode Code);
    static QString              CloseCodeToString                 (CloseCode Code);
    static OpCode               FormatForSubProtocol              (WSSubProtocol Protocol);
    static QString              SubProtocolsToString              (WSSubProtocols Protocols);
    static WSSubProtocols       SubProtocolsFromString            (const QString &Protocols);
    static QList<WSSubProtocol> SubProtocolsFromPrioritisedString (const QString &Protocols);

    TorcWebSocketReader(QTcpSocket &Socket, WSSubProtocol Protocol, bool ServerSide);
   ~TorcWebSocketReader() = default;

    const QByteArray& GetPayload         (void);
    void              Reset              (void);
    bool              CloseSent          (void);
    void              SendFrame          (OpCode Code, QByteArray &Payload);
    void              InitiateClose      (CloseCode Close, const QString &Reason);
    bool              Read               (void);
    void              EnableEcho         (void);
    void              SetSubProtocol     (WSSubProtocol Protocol);

  private:
    void              HandlePing         (QByteArray &Payload);
    void              HandlePong         (QByteArray &Payload);
    void              HandleCloseRequest (QByteArray &Close);

  private:
    enum ReadState
    {
        ReadHeader,
        Read16BitLength,
        Read64BitLength,
        ReadMask,
        ReadPayload
    };

    QTcpSocket    &m_socket;
    bool           m_serverSide;
    bool           m_closeSent;
    bool           m_closeReceived;
    bool           m_echoTest;
    WSSubProtocol  m_subProtocol;
    OpCode         m_subProtocolFrameFormat;

    // Read state
    ReadState      m_readState;
    OpCode         m_frameOpCode;
    bool           m_frameFinalFragment;
    bool           m_frameMasked;
    bool           m_haveBufferedPayload;
    QByteArray     m_bufferedPayload;
    OpCode         m_bufferedPayloadOpCode;
    quint64        m_framePayloadLength;
    quint64        m_framePayloadReadPosition;
    QByteArray     m_frameMask;
    QByteArray     m_framePayload;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TorcWebSocketReader::WSSubProtocols)
#endif // TORCWEBSOCKETREADER_H
