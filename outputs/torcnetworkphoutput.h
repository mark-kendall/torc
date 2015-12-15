#ifndef TORCNETWORKPHOUTPUT_H
#define TORCNETWORKPHOUTPUT_H

// Torc
#include "torcphoutput.h"

class TorcNetworkpHOutput : public TorcpHOutput
{
  public:
    TorcNetworkpHOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHOutput();
};

#endif // TORCNETWORKPHOUTPUT_H
