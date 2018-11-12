#ifndef TORCSWITCHINPUT_H
#define TORCSWITCHINPUT_H

// Torc
#include "torcinput.h"

class TorcSwitchInput : public TorcInput
{
  public:
    TorcSwitchInput(double Default, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcSwitchInput();

    QStringList      GetDescription (void) Q_DECL_OVERRIDE;
    TorcInput::Type  GetType        (void) Q_DECL_OVERRIDE;
    void             Start          (void) Q_DECL_OVERRIDE;
    double           ScaleValue     (double Value);
};

#endif // TORCSWITCHINPUT_H
