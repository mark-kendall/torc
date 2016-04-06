#ifndef TORCLOGICCONTROL_H
#define TORCLOGICCONTROL_H

// Torc
#include "torccontrol.h"

class TorcLogicControl : public TorcControl
{
  public:
    enum Operation
    {
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

    static TorcLogicControl::Operation StringToOperation (const QString &Operation, bool *Ok);

  public:
    TorcLogicControl(const QString &Type, const QVariantMap &Details);
   ~TorcLogicControl();

    bool                        Validate         (void);
    TorcControl::Type           GetType          (void);
    QStringList                 GetDescription   (void);
    bool                        IsPassthrough    (void);

  private:
    void                        CalculateOutput  (void);

  private:
    TorcLogicControl::Operation m_operation;

    // Reference device for controls that require a 'comparison' value.
    QString                     m_referenceDeviceId;
    QObject                    *m_referenceDevice;
};

#endif // TORCLOGICCONTROL_H
