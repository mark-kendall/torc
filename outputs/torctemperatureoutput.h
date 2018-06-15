#ifndef TORCTEMPERATUREOUTPUT_H
#define TORCTEMPERATUREOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcTemperatureOutput : public TorcOutput
{
  public:
    TorcTemperatureOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcTemperatureOutput();

    TorcOutput::Type GetType (void) Q_DECL_OVERRIDE;
};

#endif // TORCTEMPERATUREOUTPUT_H
