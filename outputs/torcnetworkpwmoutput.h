#ifndef TORCNETWORKPWMOUTPUT_H
#define TORCNETWORKPWMOUTPUT_H

// Torc
#include "torcpwmoutput.h"

class TorcNetworkPWMOutput final : public TorcPWMOutput
{
    Q_OBJECT

  public:
    TorcNetworkPWMOutput(double Default, const QVariantMap &Details);
   ~TorcNetworkPWMOutput() = default;

    QStringList GetDescription(void) override;

  public slots:
    void        SetValue (double Value) override;
};

#endif // TORCNETWORKPWMOUTPUT_H
