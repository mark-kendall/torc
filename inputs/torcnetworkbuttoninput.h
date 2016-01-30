#ifndef TORCNETWORKBUTTONINPUT_H
#define TORCNETWORKBUTTONINPUT_H

// Qt
#include <QTimer>

// Torc
#include "torcswitchinput.h"

class TorcNetworkButtonInput : public TorcSwitchInput
{
    Q_OBJECT

  public:
    TorcNetworkButtonInput(double Default, const QVariantMap &Details);
   ~TorcNetworkButtonInput();

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

#endif // TORCNETWORKBUTTONINPUT_H
