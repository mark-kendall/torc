#ifndef TORCNETWORKSWITCHOUTPUT_H
#define TORCNETWORKSWITCHOUTPUT_H

// Torc
#include "torcswitchoutput.h"

class TorcNetworkSwitchOutput : public TorcSwitchOutput
{
  public:
    TorcNetworkSwitchOutput(double Default, const QVariantMap &Details);
   ~TorcNetworkSwitchOutput();
};

#endif // TORCNETWORKSWITCHOUTPUT_H
