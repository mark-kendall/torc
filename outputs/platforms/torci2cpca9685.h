#ifndef TORCI2CPCA9685_H
#define TORCI2CPCA9685_H

// Torc
#include "../torcoutputs.h"

class TorcI2CPCA9685Channel;

class TorcI2CPCA9685
{
    friend class TorcI2CPCA9685Channel;

  public:
    TorcI2CPCA9685(int Address);
    ~TorcI2CPCA9685();

  protected:
    bool                   SetPWM (int Channel, double Value);

  private:
    int                    m_address;
    TorcI2CPCA9685Channel *m_outputs[16];
    int                    m_fd;
};

#endif // TORCI2CPCA9685_H
