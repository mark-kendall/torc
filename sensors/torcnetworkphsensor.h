#ifndef TORCNETWORKPHSENSOR_H
#define TORCNETWORKPHSENSOR_H

// Torc
#include "torcphsensor.h"

class TorcNetworkpHSensor : public TorcpHSensor
{
  public:
    TorcNetworkpHSensor(double Default, const QVariantMap &Details);
    ~TorcNetworkpHSensor();
};

#endif // TORCNETWORKPHSENSOR_H
