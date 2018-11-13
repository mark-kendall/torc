#ifndef TORCNETWORKTEMPERATUREINPUT_H
#define TORCNETWORKTEMPERATUREINPUT_H

// Torc
#include "torctemperatureinput.h"

class TorcNetworkTemperatureInput final : public TorcTemperatureInput
{
  public:
    TorcNetworkTemperatureInput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureInput();

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKTEMPERATUREINPUT_H
