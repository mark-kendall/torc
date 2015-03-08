#ifndef TORCTEMPERATURESENSOR_H
#define TORCTEMPERATURESENSOR_H

// Torc
#include "torcsensor.h"

class TorcTemperatureSensor : public TorcSensor
{
    Q_OBJECT
    Q_ENUMS(Units)

  public:
    enum Units
    {
        Celsius = 0,
        Fahrenheit
    };

  public:
    static QString UnitsToShortString(TorcTemperatureSensor::Units Units);
    static QString UnitsToLongString(TorcTemperatureSensor::Units Units);

    TorcTemperatureSensor(TorcTemperatureSensor::Units Units,
                          double Value, double RangeMinimum, double RangeMaximum,
                          const QString &ModelId, const QString &UniqueId,
                          const QVariantMap &Details);
    virtual ~TorcTemperatureSensor();

    TorcSensor::Type GetType      (void);

  private:
    double           ScaleValue   (double Value);

  private:
    TorcTemperatureSensor::Units defaultUnits;
    TorcTemperatureSensor::Units currentUnits;
};

#endif // TORCTEMPERATURESENSOR_H
