#ifndef TORCNETWORKSWITCHINPUT_H
#define TORCNETWORKSWITCHINPUT_H

// Torc
#include "torcswitchinput.h"

class TorcNetworkSwitchInput : public TorcSwitchInput
{
  public:
    TorcNetworkSwitchInput(double Default, const QVariantMap &Details);
    virtual ~TorcNetworkSwitchInput();

    QStringList GetDescription (void) Q_DECL_OVERRIDE;
    void        Start          (void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKSWITCHINPUT_H
