#ifndef TORCSWITCHOUTPUT_H
#define TORCSWITCHOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcSwitchOutput : public TorcOutput
{
  public:
    TorcSwitchOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcSwitchOutput();

    QStringList      GetDescription (void) Q_DECL_OVERRIDE;
    TorcOutput::Type GetType        (void) Q_DECL_OVERRIDE;

  public slots:
    virtual void     SetValue       (double Value) Q_DECL_OVERRIDE;
};

#endif // TORCSWITCHOUTPUT_H
