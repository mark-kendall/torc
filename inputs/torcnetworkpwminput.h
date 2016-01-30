#ifndef TORCNETWORKPWMINPUT_H
#define TORCNETWORKPWMINPUT_H

// Torc
#include "torcpwminput.h"

class TorcNetworkPWMInput : public TorcPWMInput
{
  public:
    TorcNetworkPWMInput(double Default, const QVariantMap &Details);
    ~TorcNetworkPWMInput();

    QStringList GetDescription(void);
};

#endif // TORCNETWORKPWMINPUT_H
