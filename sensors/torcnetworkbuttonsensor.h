#ifndef TORCNETWORKBUTTONSENSOR_H
#define TORCNETWORKBUTTONSENSOR_H

// Qt
#include <QTimer>

// Torc
#include "torcswitchsensor.h"

class TorcNetworkButtonSensor : public TorcSwitchSensor
{
    Q_OBJECT

  public:
    TorcNetworkButtonSensor(double Default, const QVariantMap &Details);
   ~TorcNetworkButtonSensor();

    QStringList GetDescription(void);

  signals:
    void Pushed     (void);

  public slots:
    void SetValue   (double Value);

  private slots:
    void EndPulse   (void);
    void StartTimer (void);

  private:
    QTimer m_pulseTimer;
};

#endif // TORCNETWORKBUTTONSENSOR_H
