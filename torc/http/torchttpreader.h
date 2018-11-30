#ifndef TORCHTTPREADER_H
#define TORCHTTPREADER_H

// Qt
#include <QByteArray>
#include <QTcpSocket>

class TorcHTTPReader
{
    friend class TorcWebSocket;

  public:
    TorcHTTPReader();
   ~TorcHTTPReader() = default;

    void                   TakeRequest      (QByteArray& Content, QMap<QString,QString>& Headers);
    QString                GetMethod        (void) const;
    bool                   Read             (QTcpSocket *Socket);
    bool                   IsReady          (void) const;
    bool                   HeadersComplete  (void) const;

  protected:
    void                   Reset            (void);

  private:
    bool                   m_ready;
    bool                   m_requestStarted;
    bool                   m_headersComplete;
    int                    m_headersRead;
    quint64                m_contentLength;
    quint64                m_contentReceived;
    QString                m_method;
    QByteArray             m_content;
    QMap<QString,QString>  m_headers;
};

#endif // TORCHTTPREADER_H
