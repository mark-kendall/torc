#ifndef TORCNETWORKBUTTONINPUT_H
#define TORCNETWORKBUTTONINPUT_H

// Qt
#include <QTimer>

// Torc
#include "torcnetworkswitchinput.h"

class TorcNetworkButtonInput final : public TorcNetworkSwitchInput
{
    Q_OBJECT

  public:
    TorcNetworkButtonInput(double Default, const QVariantMap &Details);
   ~TorcNetworkButtonInput() = default;

    QStringList GetDescription (void) override;
    void        Start          (void) override;

  signals:
    void Pushed     (void);

  public slots:
    void SetValue   (double Value) final;

  private slots:
    void EndPulse   (void);
    void StartTimer (void);

  private:
    QTimer m_pulseTimer;
};

#endif // TORCNETWORKBUTTONINPUT_H
