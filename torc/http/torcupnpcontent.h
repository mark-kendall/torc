#ifndef TORCUPNPCONTENT_H
#define TORCUPNPCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcUPnPContent : public TorcHTTPHandler
{
  public:
    TorcUPnPContent();

    void ProcessHTTPRequest          (TorcHTTPRequest *Request, TorcHTTPConnection* Connection);
};

#endif // TORCUPNPCONTENT_H
