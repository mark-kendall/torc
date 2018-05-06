#ifndef TORCWEBSOCKET_H
#define TORCWEBSOCKET_H

// Qt
#include <QUrl>
#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

// Torc
#include "torchttpreader.h"
#include "torcqthread.h"

class TorcHTTPRequest;
class TorcRPCRequest;
class TorcWebSocketThread;

#define TORC_JSON_RPC QString("torc.json-rpc")

class TorcWebSocket : public QObject
{
    Q_OBJECT
    Q_ENUMS(WSVersion)
    Q_ENUMS(OpCode)
    Q_ENUMS(CloseCode)
    Q_FLAGS(WSSubProtocol)

  public:
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

    Q_DECLARE_FLAGS(WSSubProtocols, WSSubProtocol);

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

    enum SocketState
    {
        DisconnectedSt = (0 << 0),
        ConnectingTo   = (1 << 1),
        ConnectedTo    = (1 << 2),
        Upgrading      = (1 << 3),
        Upgraded       = (1 << 4),
        Disconnecting  = (1 << 5),
        ErroredSt      = (1 << 6)
    };

  public:
    TorcWebSocket(TorcWebSocketThread* Parent, qintptr SocketDescriptor);
    TorcWebSocket(TorcWebSocketThread* Parent, const QHostAddress &Address, quint16 Port, bool Authenticate, WSSubProtocol Protocol = SubProtocolJSONRPC);
    ~TorcWebSocket();

    static QString  OpCodeToString        (OpCode Code);
    static QString  CloseCodeToString     (CloseCode Code);
    static QString  SubProtocolsToString  (WSSubProtocols Protocols);
    static WSSubProtocols       SubProtocolsFromString            (const QString &Protocols);
    static QList<WSSubProtocol> SubProtocolsFromPrioritisedString (const QString &Protocols);
    static QVariantList         GetSupportedSubProtocols (void);

  signals:
    void            ConnectionEstablished (void);
    void            Disconnected          (void);
    void            Disconnect            (void);

  public slots:
    void            Start                 (void);
    void            CloseSocket           (void);
    void            PropertyChanged       (void);
    bool            HandleNotification    (const QString &Method);
    void            RemoteRequest         (TorcRPCRequest *Request);
    void            CancelRequest         (TorcRPCRequest *Request);

  protected slots:
    void            ReadyRead             (void);
    void            Connected             (void);
    void            Error                 (QAbstractSocket::SocketError);
    void            SubscriberDeleted     (QObject *Subscriber);

  protected:
    bool            event                 (QEvent *Event) Q_DECL_OVERRIDE;

  private:
    Q_DISABLE_COPY(TorcWebSocket)
    void            SetState              (SocketState State);
    void            HandleUpgradeRequest  (TorcHTTPRequest &Request);
    void            SendHandshake         (void);
    void            ReadHandshake         (void);
    void            ReadHTTP              (void);
    OpCode          FormatForSubProtocol  (WSSubProtocol Protocol);
    void            SendFrame             (OpCode Code, QByteArray &Payload);
    void            HandlePing            (QByteArray &Payload);
    void            HandlePong            (QByteArray &Payload);
    void            HandleCloseRequest    (QByteArray &Close);
    void            InitiateClose         (CloseCode Close, const QString &Reason);
    void            ProcessPayload        (const QByteArray &Payload);

  private:
    enum ReadState
    {
        ReadHeader,
        Read16BitLength,
        Read64BitLength,
        ReadMask,
        ReadPayload
    };

    TorcWebSocketThread *m_parent;
    QTcpSocket      *m_socket;
    SocketState      m_socketState;
    qintptr          m_socketDescriptor;
    TorcHTTPReader   m_reader;
    bool             m_authenticate;
    bool             m_authenticated;
    QString          m_challengeResponse;
    QHostAddress     m_address;
    quint16          m_port;
    bool             m_serverSide;
    ReadState        m_readState;
    bool             m_echoTest;

    WSSubProtocol    m_subProtocol;
    OpCode           m_subProtocolFrameFormat;
    bool             m_frameFinalFragment;
    OpCode           m_frameOpCode;
    bool             m_frameMasked;
    quint64          m_framePayloadLength;
    quint64          m_framePayloadReadPosition;
    QByteArray       m_frameMask;
    QByteArray       m_framePayload;

    QByteArray      *m_bufferedPayload;
    OpCode           m_bufferedPayloadOpCode;

    bool             m_closeReceived;
    bool             m_closeSent;

    int              m_currentRequestID;
    QMap<int,TorcRPCRequest*> m_currentRequests;
    QMap<int,int>    m_requestTimers;

    QMultiMap<QString,QObject*> m_subscribers;   // client side
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TorcWebSocket::WSSubProtocols);
#endif // TORCWEBSOCKET_H
