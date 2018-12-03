#ifndef TORCNETWORKPWMINPUT_H
#define TORCNETWORKPWMINPUT_H

// Torc
#include "torcpwminput.h"

class TorcNetworkPWMInput final : public TorcPWMInput
{
    Q_OBJECT
  public:
    TorcNetworkPWMInput(double Default, const QVariantMap &Details);
    ~TorcNetworkPWMInput() = default;

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKPWMINPUT_H
