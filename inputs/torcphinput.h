#ifndef TORCPHINPUT_H
#define TORCPHINPUT_H

// Torc
#include "torcinput.h"

class TorcpHInput : public TorcInput
{
    Q_OBJECT

  public:
    TorcpHInput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcpHInput();

    QStringList      GetDescription (void) Q_DECL_OVERRIDE;
    TorcInput::Type  GetType        (void) Q_DECL_OVERRIDE;
    void             Start          (void) Q_DECL_OVERRIDE;
    double           ScaleValue     (double Value);
};

#endif // TORCPHINPUT_H
