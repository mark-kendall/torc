#ifndef TORCNETWORKSWITCHINPUT_H
#define TORCNETWORKSWITCHINPUT_H

// Torc
#include "torcswitchinput.h"

class TorcNetworkSwitchInput : public TorcSwitchInput
{
  public:
    TorcNetworkSwitchInput(double Default, const QVariantMap &Details);
    virtual ~TorcNetworkSwitchInput();

    QStringList GetDescription (void);
    void        Start          (void);
};

#endif // TORCNETWORKSWITCHINPUT_H
