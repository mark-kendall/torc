#ifndef TORCLOGICCONTROL_H
#define TORCLOGICCONTROL_H

// Torc
#include "torccontrol.h"

class TorcLogicControl : public TorcControl
{
  public:
    enum Operation
    {
        NoOperation,
        Equal,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual,
        Any,
        All,
        Average
    };

    static TorcLogicControl::Operation StringToOperation (const QString &Operation, bool *Ok);
    static QString                     OperationToString (TorcLogicControl::Operation Operation);

  public:
    TorcLogicControl(const QString &UniqueId, const QVariantMap &Details);
   ~TorcLogicControl();

    bool                        Validate         (void);
    TorcControl::Type           GetType          (void);

  private:
    void                        CalculateOutput  (void);

  private:
    TorcLogicControl::Operation m_operation;
    double                      m_operationValue;
};

#endif // TORCLOGICCONTROL_H
