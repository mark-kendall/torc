#ifndef TORCUPNPCONTENT_H
#define TORCUPNPCONTENT_H

// Torc
#include "torchttphandler.h"

#define UPNP_DIRECTORY QStringLiteral("/upnp/")

class TorcUPnPContent final: public TorcHTTPHandler
{
  public:
    TorcUPnPContent();

    void ProcessHTTPRequest (const QString &PeerAddress, int PeerPort, const QString &LocalAddress, int LocalPort, TorcHTTPRequest &Request) override;
};

#endif // TORCUPNPCONTENT_H
