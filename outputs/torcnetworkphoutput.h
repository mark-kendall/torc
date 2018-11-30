#ifndef TORCNETWORKPHOUTPUT_H
#define TORCNETWORKPHOUTPUT_H

// Torc
#include "torcphoutput.h"

class TorcNetworkpHOutput final : public TorcpHOutput
{
    Q_OBJECT

  public:
    TorcNetworkpHOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHOutput() = default;

    QStringList GetDescription(void) override;
};

#endif // TORCNETWORKPHOUTPUT_H
