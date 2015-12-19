#ifndef TORCNETWORKPWMOUTPUT_H
#define TORCNETWORKPWMOUTPUT_H

// Torc
#include "torcpwmoutput.h"

class TorcNetworkPWMOutput : public TorcPWMOutput
{
  public:
    TorcNetworkPWMOutput(double Default, const QVariantMap &Details);
   ~TorcNetworkPWMOutput();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKPWMOUTPUT_H
