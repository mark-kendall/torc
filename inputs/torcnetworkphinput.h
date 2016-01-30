#ifndef TORCNETWORKPHINPUT_H
#define TORCNETWORKPHINPUT_H

// Torc
#include "torcphinput.h"

class TorcNetworkpHInput : public TorcpHInput
{
  public:
    TorcNetworkpHInput(double Default, const QVariantMap &Details);
    ~TorcNetworkpHInput();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKPHINPUT_H
