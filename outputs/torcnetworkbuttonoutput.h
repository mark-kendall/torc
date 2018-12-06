#ifndef TORCNETWORKBUTTONOUTPUT_H
#define TORCNETWORKBUTTONOUTPUT_H

// Qt
#include <QTimer>

// Torc
#include "torcnetworkswitchoutput.h"

class TorcNetworkButtonOutput final : public TorcNetworkSwitchOutput
{
    Q_OBJECT

  public:
    TorcNetworkButtonOutput(double Default, const QVariantMap &Details);
   ~TorcNetworkButtonOutput() = default;

    QStringList GetDescription(void) override;
    TorcOutput::Type GetType (void) override;

  signals:
    void Pushed     (void);

  public slots:
    void SetValue   (double Value) override;

  private slots:
    void EndPulse   (void);
    void StartTimer (void);

  private:
    QTimer m_pulseTimer;
};

#endif // TORCNETWORKBUTTONOUTPUT_H
