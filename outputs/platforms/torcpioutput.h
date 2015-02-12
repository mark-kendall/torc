#ifndef TORCPIOUTPUT
#define TORCPIOUTPUT

// Torc
#include "torcswitchoutput.h"

class TorcPiOutput : public TorcSwitchOutput
{
  Q_OBJECT

  public:
    TorcPiOutput(int Pin);
    ~TorcPiOutput();

    void SetValue (double Value);

  private:
    int   m_pin;
};

#endif
