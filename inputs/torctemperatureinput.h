#ifndef TORCTEMPERATUREINPUT_H
#define TORCTEMPERATUREINPUT_H

// Torc
#include "torcinput.h"

class TorcTemperatureInput : public TorcInput
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
    static QString UnitsToShortString(TorcTemperatureInput::Units Units);
    static QString UnitsToLongString(TorcTemperatureInput::Units Units);

    TorcTemperatureInput(TorcTemperatureInput::Units Units,
                          double Value, double RangeMinimum, double RangeMaximum,
                          const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcTemperatureInput();

    TorcInput::Type  GetType      (void);

  private:
    double           ScaleValue   (double Value);

  private:
    TorcTemperatureInput::Units defaultUnits;
    TorcTemperatureInput::Units currentUnits;
};

#endif // TORCTEMPERATUREINPUT_H
