#ifndef TORCWEBSOCKETPOOL_H
#define TORCWEBSOCKETPOOL_H

// Qt
#include <QObject>
#include <QMutex>

// Torc
#include "torcwebsocketthread.h"

#define MAX_SOCKET_THREADS 50 // this will exclude peer websockets

class TorcWebSocketPool : public QObject
{
    Q_OBJECT

  public:
    TorcWebSocketPool();
    ~TorcWebSocketPool();

  public slots:
    void WebSocketClosed        (void);
    void IncomingConnection     (qintptr SocketDescriptor, bool Secure);

  public:
    void CloseSockets ();
    TorcWebSocketThread* TakeSocket(TorcWebSocketThread* Socket);

  private:
    QList<TorcWebSocketThread*> m_webSockets;
    QMutex                      m_webSocketsLock;
};

#endif // TORCWEBSOCKETPOOL_H
