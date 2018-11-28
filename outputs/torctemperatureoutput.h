#ifndef TORCTEMPERATUREOUTPUT_H
#define TORCTEMPERATUREOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcTemperatureOutput : public TorcOutput
{
  public:
    TorcTemperatureOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcTemperatureOutput();

    QStringList      GetDescription (void) override;
    TorcOutput::Type GetType        (void) override;
};

#endif // TORCTEMPERATUREOUTPUT_H
