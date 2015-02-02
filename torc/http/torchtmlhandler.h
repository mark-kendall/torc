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
    virtual void ProcessHTTPRequest (TorcHTTPRequest *Request, TorcHTTPConnection*);
};

#endif // TORCHTMLHANDLER_H
