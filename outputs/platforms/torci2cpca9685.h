#ifndef TORCI2CPCA9685_H
#define TORCI2CPCA9685_H

// Qt
#include <QMap>

// Torc
#include "../torcoutputs.h"
#include "torci2cbus.h"

class TorcI2CPCA9685Channel;

#define PCA9685 QString("pca9685")

class TorcI2CPCA9685 : public TorcI2CDevice
{
    friend class TorcI2CPCA9685Channel;

  public:
    TorcI2CPCA9685(int Address, const QVariantMap &Details);
    ~TorcI2CPCA9685();

  protected:
    bool                   SetPWM (int Channel, double Value);

  private:
    TorcI2CPCA9685Channel *m_outputs[16];
};

#endif // TORCI2CPCA9685_H
