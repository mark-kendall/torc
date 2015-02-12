#ifndef TORCPIGPIO_H
#define TORCPIGPIO_H

#define NUMBER_PINS 7

// Qt
#include <QMap>
#include <QMutex>

class TorcPiGPIO
{
  public:
    enum State
    {
        Unused = 0,
        Input,
        Output
    };

  public:
    TorcPiGPIO();
   ~TorcPiGPIO();

    static TorcPiGPIO* gPiGPIO;

    void               Check      (void);
    bool               ReservePin (int Pin, void* Owner, TorcPiGPIO::State InOut);
    void               ReleasePin (int Pin, void* Owner);

  private:
    QMap<int, QPair<TorcPiGPIO::State,void*> > pins;
    QMutex            *m_lock;
    bool               m_setup;
};

#endif // TORCPIGPIO_H
