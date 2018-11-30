#ifndef TORCLOGICCONTROL_H
#define TORCLOGICCONTROL_H

// Torc
#include "torcmaths.h"
#include "torccontrol.h"

class TorcLogicControl : public TorcControl
{
  public:
    enum Operation
    {
        UnknownLogicType,
        Passthrough, // no op
        Equal,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual,
        Any,
        All,
        None,
        Average,
        Toggle,
        Invert,
        Maximum,
        Minimum,
        Multiply,
        RunningAverage,
        RunningMax,
        RunningMin
    };

    static TorcLogicControl::Operation StringToOperation (const QString &Operation);

  public:
    TorcLogicControl(const QString &Type, const QVariantMap &Details);
   ~TorcLogicControl() = default;

    bool                        Validate         (void) override;
    TorcControl::Type           GetType          (void) const override;
    QStringList                 GetDescription   (void) override;
    bool                        IsPassthrough    (void) override;

  private:
    void                        CalculateOutput  (void) override;

  private:
    Q_DISABLE_COPY(TorcLogicControl)
    TorcLogicControl::Operation m_operation;

    // Reference device for controls that require a 'comparison' value (or reset for running avg/max/min)
    QString                     m_referenceDeviceId;
    QObject                    *m_referenceDevice;
    QObject                    *m_inputDevice; // for simplicity
    // trigger device for updating running average. Reference device resets.
    QString                     m_triggerDeviceId;
    QObject                    *m_triggerDevice;
    // 'running' devices
    TorcAverage<double>         m_average;
    bool                        m_firstRunningValue;
    double                      m_runningValue;
};

#endif // TORCLOGICCONTROL_H
