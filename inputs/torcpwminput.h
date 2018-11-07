#ifndef TORCPWMINPUT_H
#define TORCPWMINPUT_H

// Torc
#include "torcinput.h"

class TorcPWMInput : public TorcInput
{
    Q_OBJECT

  public:
    TorcPWMInput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcPWMInput();

    QStringList      GetDescription (void) Q_DECL_OVERRIDE;
    TorcInput::Type  GetType        (void) Q_DECL_OVERRIDE;
    void             Start          (void) Q_DECL_OVERRIDE;
    double           ScaleValue     (double Value);
};

#endif // TORCPWMINPUT_H
