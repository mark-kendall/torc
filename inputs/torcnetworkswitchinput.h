#ifndef TORCNETWORKSWITCHINPUT_H
#define TORCNETWORKSWITCHINPUT_H

// Torc
#include "torcswitchinput.h"

class TorcNetworkSwitchInput : public TorcSwitchInput
{
  public:
    TorcNetworkSwitchInput(double Default, const QVariantMap &Details);
    virtual ~TorcNetworkSwitchInput();

    virtual QStringList GetDescription(void);
};

#endif // TORCNETWORKSWITCHINPUT_H
