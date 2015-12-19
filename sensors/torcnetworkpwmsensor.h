#ifndef TORCNETWORKPWMSENSOR_H
#define TORCNETWORKPWMSENSOR_H

// Torc
#include "torcpwmsensor.h"

class TorcNetworkPWMSensor : public TorcPWMSensor
{
  public:
    TorcNetworkPWMSensor(double Default, const QVariantMap &Details);
    ~TorcNetworkPWMSensor();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKPWMSENSOR_H
