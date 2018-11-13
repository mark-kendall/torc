#ifndef TORCSWITCHOUTPUT_H
#define TORCSWITCHOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcSwitchOutput : public TorcOutput
{
  public:
    TorcSwitchOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcSwitchOutput();

    QStringList      GetDescription (void) override;
    TorcOutput::Type GetType        (void) override;

  public slots:
    virtual void     SetValue       (double Value) override;
};

#endif // TORCSWITCHOUTPUT_H
