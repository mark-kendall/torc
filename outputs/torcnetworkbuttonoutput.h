#ifndef TORCNETWORKBUTTONOUTPUT_H
#define TORCNETWORKBUTTONOUTPUT_H

// Qt
#include <QTimer>

// Torc
#include "torcnetworkswitchoutput.h"

class TorcNetworkButtonOutput : public TorcNetworkSwitchOutput
{
    Q_OBJECT
  public:
    TorcNetworkButtonOutput(double Default, const QVariantMap &Details);
   ~TorcNetworkButtonOutput();

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

#endif // TORCNETWORKBUTTONOUTPUT_H
