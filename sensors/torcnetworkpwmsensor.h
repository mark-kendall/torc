#ifndef TORCNETWORKPWMSENSOR_H
#define TORCNETWORKPWMSENSOR_H

// Torc
#include "torcpwmsensor.h"

class TorcNetworkPWMSensor : public TorcPWMSensor
{
  public:
    TorcNetworkPWMSensor(double Default, const QVariantMap &Details);
    ~TorcNetworkPWMSensor();
};

#endif // TORCNETWORKPWMSENSOR_H
