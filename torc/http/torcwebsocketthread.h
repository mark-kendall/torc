#ifndef TORCWEBSOCKETTHREAD_H
#define TORCWEBSOCKETTHREAD_H

// Qt
#include <QObject>

// Torc
#include "torcwebsocket.h"

class TorcWebSocketThread : public TorcQThread
{
    Q_OBJECT

  public:
    TorcWebSocketThread(TorcHTTPRequest *Request, QTcpSocket *Socket);
    TorcWebSocketThread(const QHostAddress &Address, quint16 Port, bool Authenticate = false, TorcWebSocket::WSSubProtocol Protocol = TorcWebSocket::SubProtocolJSONRPC);
    ~TorcWebSocketThread();

    void                Start    (void);
    void                Finish   (void);
    void                RemoteRequest         (TorcRPCRequest *Request);
    void                CancelRequest         (TorcRPCRequest *Request);

  signals:
    void                ConnectionEstablished (void);
    void                RemoteRequestSignal   (TorcRPCRequest *Request);
    void                CancelRequestSignal   (TorcRPCRequest *Request);

  private:
    TorcWebSocket      *m_webSocket;

    // incoming
    TorcHTTPRequest    *m_request;
    QTcpSocket         *m_socket;

    // outgoing
    const QHostAddress m_address;
    quint16            m_port;
    bool               m_authenticate;
    TorcWebSocket::WSSubProtocol m_protocol;
};

#endif // TORCWEBSOCKETTHREAD_H
