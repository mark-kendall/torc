#ifndef TORCI2CPCA9685_H
#define TORCI2CPCA9685_H

// Torc
#include "../torcoutputs.h"

class TorcI2CPCA9685Channel;

class TorcI2CPCA9685
{
  public:
    TorcI2CPCA9685(int Address);
    ~TorcI2CPCA9685();

  private:
    int                    m_address;
    TorcI2CPCA9685Channel *m_outputs[16];
};

#endif // TORCI2CPCA9685_H
