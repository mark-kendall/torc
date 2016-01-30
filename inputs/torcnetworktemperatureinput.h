#ifndef TORCNETWORKTEMPERATUREINPUT_H
#define TORCNETWORKTEMPERATUREINPUT_H

// Torc
#include "torctemperatureinput.h"

class TorcNetworkTemperatureInput : public TorcTemperatureInput
{
  public:
    TorcNetworkTemperatureInput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureInput();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKTEMPERATUREINPUT_H
