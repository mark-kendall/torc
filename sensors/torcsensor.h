#ifndef TORCSENSOR_H
#define TORCSENSOR_H

// Qt
#include <QMutex>
#include <QObject>

// Torc
#include "torcreferencecounted.h"
#include "http/torchttpservice.h"

#define SENSORS_DIRECTORY QString("sensors")

class TorcSensor : public QObject, public TorcHTTPService, public TorcReferenceCounter
{
    Q_OBJECT
    Q_ENUMS(Type)
    Q_CLASSINFO("Version", "1.0.0")
    Q_PROPERTY(bool     valid                   READ GetValid()                   NOTIFY   ValidChanged())
    Q_PROPERTY(double   value                   READ GetValue()                   NOTIFY   ValueChanged())
    Q_PROPERTY(double   valueScaled             READ GetValueScaled()             NOTIFY   ValueScaledChanged())
    Q_PROPERTY(double   operatingRangeMin       READ GetOperatingRangeMin()       CONSTANT)
    Q_PROPERTY(double   operatingRangeMax       READ GetOperatingRangeMax()       CONSTANT)
    Q_PROPERTY(double   operatingRangeMinScaled READ GetOperatingRangeMinScaled() NOTIFY   OperatingRangeMinScaledChanged())
    Q_PROPERTY(double   operatingRangeMaxScaled READ GetOperatingRangeMaxScaled() NOTIFY   OperatingRangeMaxScaledChanged())
    Q_PROPERTY(bool     outOfRangeLow           READ GetOutOfRangeLow()           NOTIFY   OutOfRangeLowChanged())
    Q_PROPERTY(bool     outOfRangeHigh          READ GetOutOfRangeHigh()          NOTIFY   OutOfRangeHighChanged())
    Q_PROPERTY(QString  shortUnits              READ GetShortUnits()              NOTIFY   ShortUnitsChanged())
    Q_PROPERTY(QString  longUnits               READ GetLongUnits()               NOTIFY   LongUnitsChanged())
    Q_PROPERTY(QString  modelId                 READ GetModelId()                 CONSTANT)
    Q_PROPERTY(QString  uniqueId                READ GetUniqueId()                CONSTANT)
    Q_PROPERTY(QString  userName                READ GetUserName()                WRITE    SetUserName()        NOTIFY UserNameChanged())
    Q_PROPERTY(QString  userDescription         READ GetUserDescription()         WRITE    SetUserDescription() NOTIFY UserDescriptionChanged())

  public:
    enum Type
    {
        Unknown     = 0,
        Temperature,
        pH,
        Switch,
        MaxType
    };

    static QString          TypeToString(TorcSensor::Type Type);
    static TorcSensor::Type StringToType(const QString &Type);

  public:
    TorcSensor(TorcSensor::Type Type, double Value, double RangeMinimum, double RangeMaximum,
               const QString &ShortUnits, const QString &LongUnits,
               const QString &ModelId, const QString &UniqueId);


    virtual TorcSensor::Type GetType           (void) = 0;

  protected:
    virtual ~TorcSensor();

  signals:
    void             ValidChanged              (bool   Valid);
    void             ValueChanged              (double Value);
    void             ValueScaledChanged        (double Value);
    void             OperatingRangeMinScaledChanged (double Value);
    void             OperatingRangeMaxScaledChanged (double Value);
    void             OutOfRangeLowChanged      (bool Value);
    void             OutOfRangeHighChanged     (bool Value);
    void             ShortUnitsChanged         (const QString &Units);
    void             LongUnitsChanged          (const QString &Units);
    void             UserNameChanged           (const QString &Name);
    void             UserDescriptionChanged    (const QString &Description);

  public slots:
    // TorcHTTPService
    void             SubscriberDeleted         (QObject *Subscriber);

    void             SetUserName               (const QString &Name);
    void             SetUserDescription        (const QString &Description);

    bool             GetValid                  (void);
    double           GetValue                  (void);
    double           GetValueScaled            (void);
    double           GetOperatingRangeMin      (void);
    double           GetOperatingRangeMax      (void);
    double           GetOperatingRangeMinScaled(void);
    double           GetOperatingRangeMaxScaled(void);
    bool             GetOutOfRangeLow          (void);
    bool             GetOutOfRangeHigh         (void);
    QString          GetShortUnits             (void);
    QString          GetLongUnits              (void);
    QString          GetModelId                (void);
    QString          GetUniqueId               (void);
    QString          GetUserName               (void);
    QString          GetUserDescription        (void);

  protected slots:
    void             SetValue                  (double Value);
    void             SetValid                  (bool   Valid);

  protected:
    virtual double   ScaleValue                (double Value) = 0;
    void             SetShortUnits             (const QString &Units);
    void             SetLongUnits              (const QString &Units);

  protected:
    bool             valid;
    double           value;
    double           valueScaled;
    double           operatingRangeMin;
    double           operatingRangeMax;
    double           operatingRangeMinScaled;
    double           operatingRangeMaxScaled;
    bool             outOfRangeLow;
    bool             outOfRangeHigh;
    double           alarmLow;
    double           alarmHigh;
    double           alarmLowScaled;
    double           alarmHighScaled;
    QString          shortUnits;
    QString          longUnits;
    QString          modelId;
    QString          uniqueId;
    QString          userName;
    QString          userDescription;

    QMutex          *m_lock;
};

#endif // TORCSENSOR_H
