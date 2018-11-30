#ifndef TORCNETWORKSWITCHINPUT_H
#define TORCNETWORKSWITCHINPUT_H

// Torc
#include "torcswitchinput.h"

class TorcNetworkSwitchInput : public TorcSwitchInput
{
  public:
    TorcNetworkSwitchInput(double Default, const QVariantMap &Details);
    virtual ~TorcNetworkSwitchInput() = default;

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKSWITCHINPUT_H
