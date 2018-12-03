#ifndef TORCINTEGERINPUT_H
#define TORCINTEGERINPUT_H

// Torc
#include "torcinput.h"

class TorcIntegerInput : public TorcInput
{
    Q_OBJECT

  public:
    TorcIntegerInput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcIntegerInput() = default;

    QStringList      GetDescription   (void) override;
    TorcInput::Type  GetType          (void) override;
    void             Start            (void) override;
};

#endif // TORCINTEGERINPUT_H
