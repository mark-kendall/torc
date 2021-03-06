#ifndef TORCPIGPIO_H
#define TORCPIGPIO_H

// Qt
#include <QMap>
#include <QMutex>

// Torc
#include "torccentral.h"
#include "torcpiswitchoutput.h"
#include "torcpipwmoutput.h"
#include "torcpiswitchinput.h"

#define PI_GPIO QStringLiteral("gpio")

class TorcPiGPIO : public TorcDeviceHandler
{
  public:
    TorcPiGPIO();
    ~TorcPiGPIO() = default;

    static TorcPiGPIO* gPiGPIO;

    void                       Create      (const QVariantMap &GPIO);
    void                       Destroy     (void);

  private:
    QMap<int,TorcPiSwitchInput*>  m_inputs;
    QMap<int,TorcPiSwitchOutput*> m_outputs;
    QMap<int,TorcPiPWMOutput*>    m_pwmOutputs;
    bool                          m_setup;
};

#endif // TORCPIGPIO_H
