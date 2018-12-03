#ifndef TORCNETWORKINTEGERINPUT_H
#define TORCNETWORKINTEGERINPUT_H

// Torc
#include "torcintegerinput.h"

class TorcNetworkIntegerInput final : public TorcIntegerInput
{
    Q_OBJECT
  public:
    TorcNetworkIntegerInput(double Default, const QVariantMap &Details);
    ~TorcNetworkIntegerInput() = default;

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKINTEGERINPUT_H
