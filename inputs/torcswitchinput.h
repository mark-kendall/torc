#ifndef TORCSWITCHINPUT_H
#define TORCSWITCHINPUT_H

// Torc
#include "torcinput.h"

class TorcSwitchInput : public TorcInput
{
    Q_OBJECT
  public:
    TorcSwitchInput(double Default, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcSwitchInput() = default;

    QStringList      GetDescription (void) override;
    TorcInput::Type  GetType        (void) override;
    void             Start          (void) override;
    double           ScaleValue     (double Value);
};

#endif // TORCSWITCHINPUT_H
