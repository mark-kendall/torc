#ifndef TORCPIGPIO_H
#define TORCPIGPIO_H

// this is wiringPi pins 0-6
// TODO why did I exclude pin 7 (GPIO 7, BCM GPIO 4)
// TODO detect presence of extra pins (rev2)
#define NUMBER_PINS 7

// Qt
#include <QMap>
#include <QMutex>

// Torc
#include "torccentral.h"
#include "torcpioutput.h"
#include "torcpiinput.h"

#define PI_GPIO QString("gpio")

class TorcPiGPIO : public TorcDeviceHandler
{
  public:
    TorcPiGPIO();
   ~TorcPiGPIO();

    static TorcPiGPIO* gPiGPIO;

    void                       Create      (const QVariantMap &GPIO);
    void                       Destroy     (void);

  private:
    QMap<int,TorcPiInput*>     m_inputs;
    QMap<int,TorcPiOutput*>    m_outputs;
    bool                       m_setup;
};

#endif // TORCPIGPIO_H
