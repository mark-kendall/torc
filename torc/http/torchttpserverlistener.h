#ifndef TORCHTTPSERVERADDRESS_H
#define TORCHTTPSERVERADDRESS_H

// Qt
#include <QTcpServer>

class TorcHTTPServerListener final : public QTcpServer
{
    Q_OBJECT

  public:
    TorcHTTPServerListener(QObject *Parent, const QHostAddress &Address, int Port = 0);
   ~TorcHTTPServerListener();

    bool Listen(const QHostAddress &Address, int Port = 0);

  signals:
    void NewConnection(qintptr SocketDescriptor);

  protected:
    void incomingConnection (qintptr SocketDescriptor) override final;
};

#endif // TORCHTTPSERVERADDRESS_H
