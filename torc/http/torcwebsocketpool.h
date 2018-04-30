#ifndef TORCWEBSOCKETPOOL_H
#define TORCWEBSOCKETPOOL_H

// Qt
#include <QObject>
#include <QMutex>

// Torc
#include "torcwebsocketthread.h"

class TorcWebSocketPool : public QObject
{
    Q_OBJECT

  public:
    TorcWebSocketPool();
    ~TorcWebSocketPool();

  public slots:
    void WebSocketClosed (void);
    void HandleUpgrade(TorcHTTPRequest *Request, QTcpSocket *Socket);

  public:
    void CloseSockets ();

  private:
    QList<TorcWebSocketThread*> m_webSockets;
    QMutex                      m_webSocketsLock;
};

#endif // TORCWEBSOCKETPOOL_H
