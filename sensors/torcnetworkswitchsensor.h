#ifndef TORCNETWORKSWITCHSENSOR_H
#define TORCNETWORKSWITCHSENSOR_H

// Torc
#include "torcswitchsensor.h"

class TorcNetworkSwitchSensor : public TorcSwitchSensor
{
  public:
    TorcNetworkSwitchSensor(double Default, const QString &UniqeId);
   ~TorcNetworkSwitchSensor();
};

#endif // TORCNETWORKSWITCHSENSOR_H
