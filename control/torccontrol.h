#ifndef TORCCONTROL_H
#define TORCCONTROL_H

// Qt
#include <QMap>
#include <QObject>

// Torc
#include "torcdevice.h"

class TorcControl : public QObject, public TorcDevice
{
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
        None = 0,
        Equals,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual
    };

  public:
    TorcControl(const QVariantMap &Details);
    virtual ~TorcControl();

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
    void                   SetValid               (bool Valid);

  private:
    QMap<QString,QObject*> m_inputs;
    QMap<QString,QObject*> m_outputs;
    TorcControl::Operation m_operation;
    double                 m_operationValue;
};

#endif // TORCCONTROL_H
