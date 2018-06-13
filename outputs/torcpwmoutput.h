#ifndef TORCPWMOUTPUT_H
#define TORCPWMOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcPWMOutput : public TorcOutput
{
  public:
    TorcPWMOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcPWMOutput();

    TorcOutput::Type GetType (void) Q_DECL_OVERRIDE;
};

#endif // TORCPWMOUTPUT_H
