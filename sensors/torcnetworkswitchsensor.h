#ifndef TORCNETWORKSWITCHSENSOR_H
#define TORCNETWORKSWITCHSENSOR_H

// Torc
#include "torcswitchsensor.h"

class TorcNetworkSwitchSensor : public TorcSwitchSensor
{
  public:
    TorcNetworkSwitchSensor(double Default, const QVariantMap &Details);
   ~TorcNetworkSwitchSensor();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKSWITCHSENSOR_H
