#ifndef TORCPISWITCHOUTPUT_H
#define TORCPISWITCHOUTPUT_H

// Torc
#include "torcswitchoutput.h"

class TorcPiSwitchOutput : public TorcSwitchOutput
{
  Q_OBJECT

  public:
    TorcPiSwitchOutput(int Pin, const QVariantMap &Details);
    ~TorcPiSwitchOutput();

    QStringList GetDescription(void);

  public slots:
    void        SetValue (double Value);

  private:
    int         m_pin;
};

#endif // TORCPISWITCHOUTPUT_H
