#ifndef TORCPHSENSOR_H
#define TORCPHSENSOR_H

// Torc
#include "torcsensor.h"

class TorcpHSensor : public TorcSensor
{
    Q_OBJECT

  public:
    TorcpHSensor(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcpHSensor();

    TorcSensor::Type GetType    (void);
    double           ScaleValue (double Value);
};

#endif // TORCPHSENSOR_H
