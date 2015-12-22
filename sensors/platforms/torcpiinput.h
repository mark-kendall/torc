#ifndef TORCPIINPUT_H
#define TORCPIINPUT_H

// Torc
#include "torcswitchsensor.h"

class TorcPiInput : public TorcSwitchSensor
{
    Q_OBJECT

  public:
    TorcPiInput(int Pin, const QVariantMap &Details);
    ~TorcPiInput();

    void        Start          (void);
    QStringList GetDescription (void);

  private:
    int         m_pin;
};

#endif // TORCPIINPUT_H
