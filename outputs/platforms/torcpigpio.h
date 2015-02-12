#ifndef TORCPIGPIO_H
#define TORCPIGPIO_H

#define NUMBER_PINS 7

// Qt
#include <QMap>
#include <QMutex>

// Torc
#include "torccentral.h"
#include "torcpioutput.h"
class TorcPiInput;

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
    QMutex                    *m_lock;
    bool                       m_setup;
};

#endif // TORCPIGPIO_H
