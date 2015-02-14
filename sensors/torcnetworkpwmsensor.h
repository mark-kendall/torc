#ifndef TORCNETWORKPWMSENSOR_H
#define TORCNETWORKPWMSENSOR_H

// Torc
#include "torcpwmsensor.h"

class TorcNetworkPWMSensor : public TorcPWMSensor
{
  public:
    TorcNetworkPWMSensor(double Default, const QString &UniqueId);
    ~TorcNetworkPWMSensor();
};

#endif // TORCNETWORKPWMSENSOR_H