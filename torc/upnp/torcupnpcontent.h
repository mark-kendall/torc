#ifndef TORCUPNPCONTENT_H
#define TORCUPNPCONTENT_H

// Torc
#include "torchttphandler.h"

class TorcUPnPContent Q_DECL_FINAL: public TorcHTTPHandler
{
  public:
    TorcUPnPContent();

    void ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) Q_DECL_OVERRIDE;
};

#endif // TORCUPNPCONTENT_H
