#ifndef TORCNETWORKPHINPUT_H
#define TORCNETWORKPHINPUT_H

// Torc
#include "torcphinput.h"

class TorcNetworkpHInput final : public TorcpHInput
{
    Q_OBJECT
  public:
    TorcNetworkpHInput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHInput() = default;

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKPHINPUT_H
