#ifndef TORCWEBSOCKET_H
#define TORCWEBSOCKET_H

// Qt
#include <QUrl>
#include <QTimer>
#include <QObject>
#include <QSslSocket>
#include <QHostAddress>

// Torc
#include "torchttpreader.h"
#include "torcwebsocketreader.h"
#include "torcqthread.h"

class TorcHTTPRequest;
class TorcRPCRequest;
class TorcWebSocketThread;

#define HTTP_SOCKET_TIMEOUT 30000  // 30 seconds of inactivity
#define FULL_SOCKET_TIMEOUT 300000 // 5 minutes of inactivity

class TorcWebSocket : public QSslSocket
{
    Q_OBJECT

  public:
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
    TorcWebSocket(TorcWebSocketThread* Parent, qintptr SocketDescriptor, bool Secure);
    TorcWebSocket(TorcWebSocketThread* Parent, const QHostAddress &Address, quint16 Port, bool Secure, bool Authenticate,
                  TorcWebSocketReader::WSSubProtocol Protocol = TorcWebSocketReader::SubProtocolJSONRPC);
    ~TorcWebSocket();

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

    // SSL
    void            Encrypted             (void);
    void            SSLErrors             (const QList<QSslError> &Errors);

  protected slots:
    void            ReadyRead             (void);
    void            Connected             (void);
    void            Error                 (QAbstractSocket::SocketError);
    void            SubscriberDeleted     (QObject *Subscriber);
    void            TimedOut              (void);
    void            BytesWritten          (qint64);

  protected:
    bool            event                 (QEvent *Event) Q_DECL_OVERRIDE;

  private:
    Q_DISABLE_COPY(TorcWebSocket)
    void            SetState              (SocketState State);
    void            HandleUpgradeRequest  (TorcHTTPRequest &Request);
    void            SendHandshake         (void);
    void            ReadHandshake         (void);
    void            ReadHTTP              (void);
    void            ProcessPayload        (const QByteArray &Payload);

  private:
    TorcWebSocketThread *m_parent;
    bool             m_secure;
    SocketState      m_socketState;
    qintptr          m_socketDescriptor;
    QTimer           m_watchdogTimer;
    TorcHTTPReader   m_reader;
    TorcWebSocketReader m_wsReader;
    bool             m_authenticate;
    bool             m_authenticated;
    QString          m_challengeResponse;
    QHostAddress     m_address;
    quint16          m_port;
    bool             m_serverSide;

    TorcWebSocketReader::WSSubProtocol m_subProtocol;
    TorcWebSocketReader::OpCode m_subProtocolFrameFormat;

    int              m_currentRequestID;
    QMap<int,TorcRPCRequest*> m_currentRequests;
    QMap<int,int>    m_requestTimers;

    QMultiMap<QString,QObject*> m_subscribers;   // client side
};

#endif // TORCWEBSOCKET_H
