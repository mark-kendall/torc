#ifndef TORCPIPWMOUTPUT_H
#define TORCPIPWMOUTPUT_H

// Torc
#include "torcpwmoutput.h"

#define TORC_HWPWM_PIN 1

class TorcPiPWMOutput : public TorcPWMOutput
{
    Q_OBJECT

  public:
    TorcPiPWMOutput(int Pin, const QVariantMap &Details);
    ~TorcPiPWMOutput();

    QStringList GetDescription(void);

  public slots:
    void        SetValue (double Value);

  private:
    int         m_pin;
};

#endif // TORCPIPWMOUTPUT_H
