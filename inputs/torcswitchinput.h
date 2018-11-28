#ifndef TORCSWITCHINPUT_H
#define TORCSWITCHINPUT_H

// Torc
#include "torcinput.h"

class TorcSwitchInput : public TorcInput
{
  public:
    TorcSwitchInput(double Default, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcSwitchInput();

    QStringList      GetDescription (void) override;
    TorcInput::Type  GetType        (void) override;
    void             Start          (void) override;
    double           ScaleValue     (double Value);
};

#endif // TORCSWITCHINPUT_H
