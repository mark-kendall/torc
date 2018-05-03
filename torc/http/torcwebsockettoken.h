#ifndef TORCWEBSOCKETTOKEN_H
#define TORCWEBSOCKETTOKEN_H

// Torc
#include "torchttprequest.h"

class TorcWebSocketToken
{
  public:
    static QString GetWebSocketToken(const QString &Host, TorcHTTPRequest *Request, const QString &Current = QString());
};

#endif // TORCWEBSOCKETTOKEN_H
