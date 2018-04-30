#ifndef TORCHTTPCONNECTION_H
#define TORCHTTPCONNECTION_H

// Qt
#include <QBuffer>
#include <QObject>
#include <QRunnable>
#include <QTcpSocket>

// Torc
#include "torchttpserver.h"

class TorcHTTPConnection : public QRunnable
{
  public:
    TorcHTTPConnection(TorcHTTPServer *Parent, qintptr SocketDescriptor, int *Abort);
    virtual ~TorcHTTPConnection();

  public:
    void                     run            (void) Q_DECL_OVERRIDE;
    QTcpSocket*              GetSocket      (void);
    TorcHTTPServer*          GetServer      (void);

  protected:
    int                     *m_abort;
    TorcHTTPServer          *m_server;
    qintptr                  m_socketDescriptor;
    QTcpSocket              *m_socket;
};

#endif // TORCHTTPCONNECTION_H
