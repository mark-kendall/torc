#ifndef TORCNETWORKSWITCHOUTPUT_H
#define TORCNETWORKSWITCHOUTPUT_H

// Torc
#include "torcswitchoutput.h"

class TorcNetworkSwitchOutput : public TorcSwitchOutput
{
    Q_OBJECT

  public:
    TorcNetworkSwitchOutput(double Default, const QVariantMap &Details);
    virtual ~TorcNetworkSwitchOutput();

    virtual QStringList GetDescription(void);
};

#endif // TORCNETWORKSWITCHOUTPUT_H
