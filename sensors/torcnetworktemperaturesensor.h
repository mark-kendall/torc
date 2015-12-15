#ifndef TORCNETWORKTEMPERATURESENSOR_H
#define TORCNETWORKTEMPERATURESENSOR_H

// Torc
#include "torctemperaturesensor.h"

class TorcNetworkTemperatureSensor : public TorcTemperatureSensor
{
  public:
    TorcNetworkTemperatureSensor(double Default, const QVariantMap &Details);
    ~TorcNetworkTemperatureSensor();
};

#endif // TORCNETWORKTEMPERATURESENSOR_H
