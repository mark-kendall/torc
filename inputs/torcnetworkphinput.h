#ifndef TORCNETWORKPHINPUT_H
#define TORCNETWORKPHINPUT_H

// Torc
#include "torcphinput.h"

class TorcNetworkpHInput Q_DECL_FINAL : public TorcpHInput
{
  public:
    TorcNetworkpHInput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHInput();

    QStringList GetDescription (void) Q_DECL_OVERRIDE;
    void        Start          (void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKPHINPUT_H
