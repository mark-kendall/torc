#ifndef TORCTEMPERATUREINPUT_H
#define TORCTEMPERATUREINPUT_H

// Torc
#include "torcinput.h"

class TorcTemperatureInput : public TorcInput
{
    Q_OBJECT

  public:
    TorcTemperatureInput(double Value, double RangeMinimum, double RangeMaximum,
                         const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcTemperatureInput() = default;

    QStringList      GetDescription   (void) override;
    TorcInput::Type  GetType          (void) override;
    void             Start            (void) override;

    static double CelsiusToFahrenheit (double Value);
    static double FahrenheitToCelsius (double Value);
};

#endif // TORCTEMPERATUREINPUT_H
