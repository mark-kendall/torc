#ifndef TORCNETWORKTEMPERATUREINPUT_H
#define TORCNETWORKTEMPERATUREINPUT_H

// Torc
#include "torctemperatureinput.h"

class TorcNetworkTemperatureInput : public TorcTemperatureInput
{
  public:
    TorcNetworkTemperatureInput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureInput();

    QStringList GetDescription (void);
    void        Start          (void);
};

#endif // TORCNETWORKTEMPERATUREINPUT_H
