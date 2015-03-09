#ifndef TORCPWMSENSOR_H
#define TORCPWMSENSOR_H

// Torc
#include "torcsensor.h"

class TorcPWMSensor : public TorcSensor
{
    Q_OBJECT

  public:
    TorcPWMSensor(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcPWMSensor();

    TorcSensor::Type GetType    (void);
    double           ScaleValue (double Value);
};

#endif // TORCPWMSENSOR_H
