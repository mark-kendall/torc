#ifndef TORCNETWORKPHINPUT_H
#define TORCNETWORKPHINPUT_H

// Torc
#include "torcphinput.h"

class TorcNetworkpHInput final : public TorcpHInput
{
  public:
    TorcNetworkpHInput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHInput();

    QStringList GetDescription (void) override;
    void        Start          (void) override;
};

#endif // TORCNETWORKPHINPUT_H
