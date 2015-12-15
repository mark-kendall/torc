#ifndef TORCPHOUTPUT_H
#define TORCPHOUTPUT_H

// Torc
#include "torcoutput.h"

class TorcpHOutput : public TorcOutput
{
    Q_OBJECT

  public:
    TorcpHOutput(double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcpHOutput();

    TorcOutput::Type GetType (void);
};

#endif // TORCPHOUTPUT_H
