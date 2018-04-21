#ifndef TORCHTTPCONNECTION_H
#define TORCHTTPCONNECTION_H

// Qt
#include <QBuffer>
#include <QObject>
#include <QRunnable>
#include <QTcpSocket>

// Torc
#include "torchttpserver.h"

class TorcHTTPRequest;

class TorcHTTPReader
{
    friend class TorcHTTPConnection;

  public:
    TorcHTTPReader();
   ~TorcHTTPReader();

    void                   TakeRequest      (QByteArray*& Content, QMap<QString,QString>*& Headers);
    QString                GetMethod        (void);
    bool                   Read             (QTcpSocket *Socket, int *Abort);
    bool                   IsReady          (void);
    bool                   HeadersComplete  (void);

  protected:
    void                   Reset            (void);

  private:
    // disable copy and assignment constructors
    TorcHTTPReader(const TorcHTTPReader &) Q_DECL_EQ_DELETE;
    TorcHTTPReader &operator=(const TorcHTTPReader &) Q_DECL_EQ_DELETE;

    bool                   m_ready;
    bool                   m_requestStarted;
    bool                   m_headersComplete;
    int                    m_headersRead;
    quint64                m_contentLength;
    quint64                m_contentReceived;
    QString                m_method;
    QByteArray            *m_content;
    QMap<QString,QString> *m_headers;
};

class TorcHTTPConnection : public QRunnable
{
  public:
    TorcHTTPConnection(TorcHTTPServer *Parent, qintptr SocketDescriptor, int *Abort);
    virtual ~TorcHTTPConnection();

  public:
    void                     run            (void);
    QTcpSocket*              GetSocket      (void);
    TorcHTTPServer*          GetServer      (void);

  protected:
    int                     *m_abort;
    TorcHTTPServer          *m_server;
    qintptr                  m_socketDescriptor;
    QTcpSocket              *m_socket;
};

#endif // TORCHTTPCONNECTION_H
