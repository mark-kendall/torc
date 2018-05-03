#ifndef TORCLOGICCONTROL_H
#define TORCLOGICCONTROL_H

// Torc
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
        Multiply
    };

    static TorcLogicControl::Operation StringToOperation (const QString &Operation);

  public:
    TorcLogicControl(const QString &Type, const QVariantMap &Details);
   ~TorcLogicControl();

    bool                        Validate         (void) Q_DECL_OVERRIDE;
    TorcControl::Type           GetType          (void) const Q_DECL_OVERRIDE;
    QStringList                 GetDescription   (void) Q_DECL_OVERRIDE;
    bool                        IsPassthrough    (void) Q_DECL_OVERRIDE;

  private:
    void                        CalculateOutput  (void) Q_DECL_OVERRIDE;

  private:
    Q_DISABLE_COPY(TorcLogicControl)
    TorcLogicControl::Operation m_operation;

    // Reference device for controls that require a 'comparison' value.
    QString                     m_referenceDeviceId;
    QObject                    *m_referenceDevice;

};

#endif // TORCLOGICCONTROL_H
