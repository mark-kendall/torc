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
    TorcHTTPConnection(qintptr SocketDescriptor, int *Abort);
    virtual ~TorcHTTPConnection();

  public:
    void                     run            (void) Q_DECL_OVERRIDE;
    QTcpSocket*              GetSocket      (void);

  protected:
    int                     *m_abort;
    qintptr                  m_socketDescriptor;
    QTcpSocket              *m_socket;
};

#endif // TORCHTTPCONNECTION_H
