#ifndef TORCNETWORKPHOUTPUT_H
#define TORCNETWORKPHOUTPUT_H

// Torc
#include "torcphoutput.h"

class TorcNetworkpHOutput : public TorcpHOutput
{
    Q_OBJECT

  public:
    TorcNetworkpHOutput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHOutput();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKPHOUTPUT_H
