#ifndef TORCNETWORKTEMPERATUREOUTPUT_H
#define TORCNETWORKTEMPERATUREOUTPUT_H

// Torc
#include "torctemperatureoutput.h"

class TorcNetworkTemperatureOutput : public TorcTemperatureOutput
{
  public:
    TorcNetworkTemperatureOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureOutput();
};

#endif // TORCNETWORKTEMPERATUREOUTPUT_H
