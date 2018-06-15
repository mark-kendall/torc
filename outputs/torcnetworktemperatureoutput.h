#ifndef TORCNETWORKTEMPERATUREOUTPUT_H
#define TORCNETWORKTEMPERATUREOUTPUT_H

// Torc
#include "torctemperatureoutput.h"

class TorcNetworkTemperatureOutput Q_DECL_FINAL : public TorcTemperatureOutput
{
    Q_OBJECT

  public:
    TorcNetworkTemperatureOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureOutput();

    QStringList GetDescription(void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKTEMPERATUREOUTPUT_H
