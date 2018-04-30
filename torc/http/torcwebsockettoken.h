#ifndef TORCWEBSOCKETTOKEN_H
#define TORCWEBSOCKETTOKEN_H

// Torc
#include "torchttprequest.h"
#include "torchttpconnection.h"

class TorcWebSocketToken
{
  public:
    static QString GetWebSocketToken(TorcHTTPConnection *Connection, TorcHTTPRequest *Request, const QString &Current = QString());
};

#endif // TORCWEBSOCKETTOKEN_H
