#ifndef TORCNETWORKTEMPERATUREOUTPUT_H
#define TORCNETWORKTEMPERATUREOUTPUT_H

// Torc
#include "torctemperatureoutput.h"

class TorcNetworkTemperatureOutput final : public TorcTemperatureOutput
{
    Q_OBJECT

  public:
    TorcNetworkTemperatureOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureOutput() = default;

    QStringList GetDescription(void) override;
};

#endif // TORCNETWORKTEMPERATUREOUTPUT_H
