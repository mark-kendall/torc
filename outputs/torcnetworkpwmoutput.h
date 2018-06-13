#ifndef TORCNETWORKPWMOUTPUT_H
#define TORCNETWORKPWMOUTPUT_H

// Torc
#include "torcpwmoutput.h"

class TorcNetworkPWMOutput Q_DECL_FINAL : public TorcPWMOutput
{
    Q_OBJECT

  public:
    TorcNetworkPWMOutput(double Default, const QVariantMap &Details);
   ~TorcNetworkPWMOutput();

    QStringList GetDescription(void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKPWMOUTPUT_H
