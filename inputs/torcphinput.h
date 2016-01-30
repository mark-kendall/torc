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

    TorcInput::Type  GetType    (void);
    double           ScaleValue (double Value);
};

#endif // TORCPHINPUT_H
