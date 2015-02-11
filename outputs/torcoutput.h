#ifndef TORCOUTPUT_H
#define TORCOUTPUT_H

// Qt
#include <QMutex>
#include <QObject>

// Torc
#include "torcreferencecounted.h"
#include "torchttpservice.h"

#define OUTPUTS_DIRECTORY QString("outputs")

class TorcOutput : public QObject, public TorcHTTPService, public TorcReferenceCounter
{
    Q_OBJECT
    Q_ENUMS(Type)
    Q_CLASSINFO("Version",        "1.0.0")
    Q_PROPERTY(double   value             READ GetValue()              WRITE SetValue()              NOTIFY ValueChanged())
    Q_PROPERTY(QString  modelId           READ GetModelId()            CONSTANT)
    Q_PROPERTY(QString  uniqueId          READ GetUniqueId()           CONSTANT)
    Q_PROPERTY(QString  userName          READ GetUserName()           WRITE    SetUserName()        NOTIFY UserNameChanged())
    Q_PROPERTY(QString  userDescription   READ GetUserDescription()    WRITE    SetUserDescription() NOTIFY UserDescriptionChanged())

  public:
    enum Type
    {
        Unknown    = 0,
        Switch,
        PWM,
        MaxType
     };

    static QString          TypeToString(TorcOutput::Type Type);
    static TorcOutput::Type StringToType(const QString &Type);

  public:
    TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QString &UniqueId);
    virtual ~TorcOutput();

    virtual TorcOutput::Type GetType (void) = 0;

  public slots:
    // TorcHTTPService
    void             SubscriberDeleted         (QObject *Subscriber);

    double           GetValue               (void);
    QString          GetModelId             (void);
    QString          GetUniqueId            (void);
    QString          GetUserName            (void);
    QString          GetUserDescription     (void);

    virtual void     SetValue               (double Value);
    void             SetUserName            (const QString &Name);
    void             SetUserDescription     (const QString &Description);

  signals:
    void             ValueChanged           (double Value);
    void             UserNameChanged        (const QString &Name);
    void             UserDescriptionChanged (const QString &Description);


  protected:
    double           value;
    QString          modelId;
    QString          uniqueId;
    QString          userName;
    QString          userDescription;

    QMutex          *m_lock;
};

#endif // TORCOUTPUT_H
