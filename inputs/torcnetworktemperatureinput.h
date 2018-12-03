#ifndef TORCNETWORKTEMPERATUREINPUT_H
#define TORCNETWORKTEMPERATUREINPUT_H

// Torc
#include "torctemperatureinput.h"

class TorcNetworkTemperatureInput final : public TorcTemperatureInput
{
    Q_OBJECT
  public:
    TorcNetworkTemperatureInput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureInput() = default;

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKTEMPERATUREINPUT_H
