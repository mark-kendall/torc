#ifndef TORCCONTROL_H
#define TORCCONTROL_H

// Qt
#include <QMap>
#include <QObject>
#include <QStringList>

// Torc
#include "torcdevice.h"

class TorcControl : public TorcDevice
{
    friend class TorcControls;

    Q_OBJECT
    Q_ENUMS(Operation)
    Q_PROPERTY(bool    valid           READ GetValid()           NOTIFY ValidChanged())
    Q_PROPERTY(double  value           READ GetValue()           NOTIFY ValueChanged())
    Q_PROPERTY(QString uniqueId        READ GetUniqueId()        CONSTANT)
    Q_PROPERTY(QString userName        READ GetUserName()        WRITE SetUserName()        NOTIFY UserNameChanged())
    Q_PROPERTY(QString userDescription READ GetUserDescription() WRITE SetUserDescription() NOTIFY UserDescriptionChanged())

  public:
    enum Operation
    {
        None,
        Equal,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual,
        Any,
        All,
        Average
    };

    static TorcControl::Operation StringToOperation (const QString &Operation);
    static QString                OperationToString (TorcControl::Operation Operation);

  protected:
    TorcControl(const QString &UniqueId, const QStringList &Inputs, const QStringList &Outputs,
                TorcControl::Operation Operation, double OperationValue);
    virtual ~TorcControl();

  public:
    void                   Validate               (void);

  public slots:
    void                   InputValueChanged      (double Value);
    void                   InputValidChanged      (bool   Valid);

    bool                   GetValid               (void);
    double                 GetValue               (void);
    QString                GetUniqueId            (void);
    QString                GetUserName            (void);
    QString                GetUserDescription     (void);

    void                   SetUserName            (const QString &Name);
    void                   SetUserDescription     (const QString &Description);

  signals:
    void                   ValidChanged           (bool  Valid);
    void                   ValueChanged           (double Value);
    void                   UserNameChanged        (const QString &Name);
    void                   UserDescriptionChanged (const QString &Description);

  private:
    void                   InputValidChangedPriv  (QObject* Input, bool Valid);
    void                   CheckInputValues       (void);
    void                   SetValue               (double Value);
    void                   SetValid               (bool Valid);
    void                   CalculateOutput        (void);

  private:
    bool                   m_validated;
    QStringList            m_inputList;
    QStringList            m_outputList;
    QMap<QObject*,QString> m_inputs;
    QMap<QObject*,QString> m_outputs;
    QMap<QObject*,double>  m_inputValues;
    QMap<QObject*,bool>    m_inputValids;
    TorcControl::Operation m_operation;
    double                 m_operationValue;
    bool                   m_allInputsValid;
};

#endif // TORCCONTROL_H
