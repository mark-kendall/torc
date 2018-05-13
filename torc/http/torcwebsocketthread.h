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
    TorcWebSocketThread (qintptr SocketDescriptor, bool Secure);
    TorcWebSocketThread (const QHostAddress &Address, quint16 Port, bool Secure, bool Authenticate = false,
                         TorcWebSocketReader::WSSubProtocol Protocol = TorcWebSocketReader::SubProtocolJSONRPC);
    ~TorcWebSocketThread();

    void                Start    (void) Q_DECL_OVERRIDE;
    void                Finish   (void) Q_DECL_OVERRIDE;
    void                RemoteRequest         (TorcRPCRequest *Request);
    void                CancelRequest         (TorcRPCRequest *Request);
    bool                IsSecure              (void);

  signals:
    void                ConnectionEstablished (void);
    void                RemoteRequestSignal   (TorcRPCRequest *Request);
    void                CancelRequestSignal   (TorcRPCRequest *Request);

  private:
    TorcWebSocket      *m_webSocket;
    bool                m_secure;
    // incoming
    qintptr             m_socketDescriptor;

    // outgoing
    const QHostAddress  m_address;
    quint16             m_port;
    bool                m_authenticate;
    TorcWebSocketReader::WSSubProtocol m_protocol;

  private:
    Q_DISABLE_COPY(TorcWebSocketThread)
};

#endif // TORCWEBSOCKETTHREAD_H
