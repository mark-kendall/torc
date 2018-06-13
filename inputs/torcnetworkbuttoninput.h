#ifndef TORCNETWORKBUTTONINPUT_H
#define TORCNETWORKBUTTONINPUT_H

// Qt
#include <QTimer>

// Torc
#include "torcnetworkswitchinput.h"

class TorcNetworkButtonInput Q_DECL_FINAL : public TorcNetworkSwitchInput
{
    Q_OBJECT

  public:
    TorcNetworkButtonInput(double Default, const QVariantMap &Details);
   ~TorcNetworkButtonInput();

    QStringList GetDescription (void) Q_DECL_OVERRIDE;
    void        Start          (void) Q_DECL_OVERRIDE;

  signals:
    void Pushed     (void);

  public slots:
    void SetValue   (double Value) Q_DECL_FINAL;

  private slots:
    void EndPulse   (void);
    void StartTimer (void);

  private:
    QTimer m_pulseTimer;
};

#endif // TORCNETWORKBUTTONINPUT_H
