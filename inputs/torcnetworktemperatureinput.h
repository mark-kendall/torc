#ifndef TORCNETWORKTEMPERATUREINPUT_H
#define TORCNETWORKTEMPERATUREINPUT_H

// Torc
#include "torctemperatureinput.h"

class TorcNetworkTemperatureInput Q_DECL_FINAL : public TorcTemperatureInput
{
  public:
    TorcNetworkTemperatureInput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureInput();

    QStringList GetDescription (void) Q_DECL_OVERRIDE;
    void        Start          (void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKTEMPERATUREINPUT_H
