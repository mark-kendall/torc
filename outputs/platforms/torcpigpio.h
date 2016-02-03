#ifndef TORCPIGPIO_H
#define TORCPIGPIO_H

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
    QMap<int,TorcPiSwitchInput*>  m_inputs;
    QMap<int,TorcPiSwitchOutput*> m_outputs;
    bool                          m_setup;
};

#endif // TORCPIGPIO_H
