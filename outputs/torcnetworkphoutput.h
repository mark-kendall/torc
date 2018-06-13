#ifndef TORCNETWORKPHOUTPUT_H
#define TORCNETWORKPHOUTPUT_H

// Torc
#include "torcphoutput.h"

class TorcNetworkpHOutput Q_DECL_FINAL : public TorcpHOutput
{
    Q_OBJECT

  public:
    TorcNetworkpHOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHOutput();

    QStringList GetDescription(void) Q_DECL_OVERRIDE;
};

#endif // TORCNETWORKPHOUTPUT_H
