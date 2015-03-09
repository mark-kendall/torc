#ifndef TORCSWITCHSENSOR_H
#define TORCSWITCHSENSOR_H

// Torc
#include "torcsensor.h"
class TorcSwitchSensor : public TorcSensor
{
  public:
    TorcSwitchSensor(double Default, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcSwitchSensor();

    TorcSensor::Type GetType    (void);
    double           ScaleValue (double Value);
};

#endif // TORCSWITCHSENSOR_H
