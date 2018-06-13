#ifndef TORCNETWORKPWMINPUT_H
#define TORCNETWORKPWMINPUT_H

// Torc
#include "torcpwminput.h"

class TorcNetworkPWMInput Q_DECL_FINAL : public TorcPWMInput
{
  public:
    TorcNetworkPWMInput(double Default, const QVariantMap &Details);
    ~TorcNetworkPWMInput();

    QStringList GetDescription (void) Q_DECL_OVERRIDE;
    void        Start          (void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKPWMINPUT_H
