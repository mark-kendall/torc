#ifndef TORCPWMINPUT_H
#define TORCPWMINPUT_H

// Torc
#include "torcinput.h"

class TorcPWMInput : public TorcInput
{
    Q_OBJECT

  public:
    TorcPWMInput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcPWMInput() = default;

    QStringList      GetDescription (void) override;
    TorcInput::Type  GetType        (void) override;
    void             Start          (void) override;
    double           ScaleValue     (double Value);
};

#endif // TORCPWMINPUT_H
