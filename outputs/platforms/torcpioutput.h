#ifndef TORCPIOUTPUT
#define TORCPIOUTPUT

// Torc
#include "torcswitchoutput.h"

class TorcPiOutput : public TorcSwitchOutput
{
  Q_OBJECT

  public:
    TorcPiOutput(int Pin, const QString &UniqueId);
    ~TorcPiOutput();

    void SetValue (double Value);

  private:
    int   m_pin;
};

#endif
