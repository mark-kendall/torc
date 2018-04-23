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

    bool                        Validate         (void);
    TorcControl::Type           GetType          (void) const;
    QStringList                 GetDescription   (void);
    bool                        IsPassthrough    (void) const;

  private:
    void                        CalculateOutput  (void);

  private:
    TorcLogicControl::Operation m_operation;

    // Reference device for controls that require a 'comparison' value.
    QString                     m_referenceDeviceId;
    QObject                    *m_referenceDevice;
};

#endif // TORCLOGICCONTROL_H
