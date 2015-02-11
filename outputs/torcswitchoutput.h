#ifndef TORCSWITCHOUTPUT_H
#define TORCSWITCHOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcSwitchOutput : public TorcOutput
{
  public:
    TorcSwitchOutput(double Value, const QString &ModelId, const QString &UniqueId);
    virtual ~TorcSwitchOutput();

    TorcOutput::Type GetType (void);

  public slots:
    virtual void SetValue(double Value);
};

#endif // TORCSWITCHOUTPUT_H
