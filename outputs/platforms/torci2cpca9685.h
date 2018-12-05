#ifndef TORCI2CPCA9685_H
#define TORCI2CPCA9685_H

// Qt
#include <QMap>

// Torc
#include "../torcpwmoutput.h"
#include "../torcoutputs.h"
#include "torci2cbus.h"

#define PCA9685 QStringLiteral("pca9685")

class TorcI2CPCA9685;

class TorcI2CPCA9685Channel : public TorcPWMOutput
{
    Q_OBJECT

  public:
    TorcI2CPCA9685Channel(int Number, TorcI2CPCA9685 *Parent, const QVariantMap &Details);
    ~TorcI2CPCA9685Channel();

    QStringList GetDescription(void);

  public slots:
    void SetValue (double Value);

  private:
    int             m_channelNumber;
    int             m_channelValue;
    TorcI2CPCA9685 *m_parent;

  private:
    Q_DISABLE_COPY(TorcI2CPCA9685Channel)
};

class TorcI2CPCA9685 : public TorcI2CDevice
{
    friend class TorcI2CPCA9685Channel;

  public:
    TorcI2CPCA9685(int Address, const QVariantMap &Details);
    ~TorcI2CPCA9685();

  protected:
    bool                   SetPWM (int Channel, int Value);

  private:
    Q_DISABLE_COPY(TorcI2CPCA9685)
    TorcI2CPCA9685Channel *m_outputs[16];
};

#endif // TORCI2CPCA9685_H
