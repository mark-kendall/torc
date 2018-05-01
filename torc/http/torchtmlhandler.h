#ifndef TORCHTMLHANDLER_H
#define TORCHTMLHANDLER_H

// Qt
#include <QCoreApplication>

// Torc
#include "torchttphandler.h"

class TorcHTTPServer;
class TorcHTTPRequest;
class TorcHTTPConnection;

class TorcHTMLHandler : public TorcHTTPHandler
{
    Q_DECLARE_TR_FUNCTIONS(TorcHTMLHandler)

  public:
    TorcHTMLHandler(const QString &Path, const QString &Name);
    virtual void ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest *Request) Q_DECL_OVERRIDE;

  private:
    QString     m_pathToContent;
    QStringList m_allowedFiles;
};

#endif // TORCHTMLHANDLER_H
