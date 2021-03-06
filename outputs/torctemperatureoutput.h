#ifndef TORCTEMPERATUREOUTPUT_H
#define TORCTEMPERATUREOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcTemperatureOutput : public TorcOutput
{
    Q_OBJECT
  public:
    TorcTemperatureOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcTemperatureOutput() = default;

    QStringList      GetDescription (void) override;
    TorcOutput::Type GetType        (void) override;
};

#endif // TORCTEMPERATUREOUTPUT_H
