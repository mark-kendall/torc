#ifndef TORCPIGPIO_H
#define TORCPIGPIO_H

#define NUMBER_PINS 7

// Qt
#include <QMap>
#include <QMutex>

// Torc
#include "torcpioutput.h"
class TorcPiInput;

class TorcPiGPIO
{
  public:
    TorcPiGPIO();
   ~TorcPiGPIO();

    static TorcPiGPIO* gPiGPIO;

    void                       SetupPins   (const QVariantMap &GPIO);
    void                       CleanupPins (void); 

  private:
    void                       Check       (void);

  private:
    QMap<int,TorcPiInput*>    m_inputs;
    QMap<int,TorcPiOutput*>     m_outputs;
    QMutex                    *m_lock;
    bool                       m_setup;
};

#endif // TORCPIGPIO_H
