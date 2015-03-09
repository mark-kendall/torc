#ifndef TORCSENSOR_H
#define TORCSENSOR_H

// Qt
#include <QMutex>
#include <QObject>

// Torc
#include "http/torchttpservice.h"
#include "torcdevice.h"

#define SENSORS_DIRECTORY QString("sensors")

class TorcSensor : public TorcDevice, public TorcHTTPService
{
    Q_OBJECT
    Q_ENUMS(Type)
    Q_CLASSINFO("Version", "1.0.0")
    Q_PROPERTY(double   valueScaled             READ GetValueScaled()             NOTIFY   ValueScaledChanged())
    Q_PROPERTY(double   operatingRangeMin       READ GetOperatingRangeMin()       CONSTANT)
    Q_PROPERTY(double   operatingRangeMax       READ GetOperatingRangeMax()       CONSTANT)
    Q_PROPERTY(double   operatingRangeMinScaled READ GetOperatingRangeMinScaled() NOTIFY   OperatingRangeMinScaledChanged())
    Q_PROPERTY(double   operatingRangeMaxScaled READ GetOperatingRangeMaxScaled() NOTIFY   OperatingRangeMaxScaledChanged())
    Q_PROPERTY(bool     outOfRangeLow           READ GetOutOfRangeLow()           NOTIFY   OutOfRangeLowChanged())
    Q_PROPERTY(bool     outOfRangeHigh          READ GetOutOfRangeHigh()          NOTIFY   OutOfRangeHighChanged())
    Q_PROPERTY(QString  shortUnits              READ GetShortUnits()              NOTIFY   ShortUnitsChanged())
    Q_PROPERTY(QString  longUnits               READ GetLongUnits()               NOTIFY   LongUnitsChanged())

  public:
    enum Type
    {
        Unknown     = 0,
        Temperature,
        pH,
        Switch,
        PWM,
        MaxType
    };

    static QString          TypeToString(TorcSensor::Type Type);
    static TorcSensor::Type StringToType(const QString &Type);

  public:
    TorcSensor(TorcSensor::Type Type, double Value, double RangeMinimum, double RangeMaximum,
               const QString &ShortUnits, const QString &LongUnits,
               const QString &ModelId,    const QVariantMap &Details);


    virtual TorcSensor::Type GetType           (void) = 0;
    void             Start                     (void);

  protected:
    virtual ~TorcSensor();

  signals:
    void             ValueScaledChanged        (double Value);
    void             OperatingRangeMinScaledChanged (double Value);
    void             OperatingRangeMaxScaledChanged (double Value);
    void             OutOfRangeLowChanged      (bool Value);
    void             OutOfRangeHighChanged     (bool Value);
    void             ShortUnitsChanged         (const QString &Units);
    void             LongUnitsChanged          (const QString &Units);

  public slots:
    // TorcHTTPService
    void             SubscriberDeleted         (QObject *Subscriber);

    void             SetValue                  (double Value);

    double           GetValueScaled            (void);
    double           GetOperatingRangeMin      (void);
    double           GetOperatingRangeMax      (void);
    double           GetOperatingRangeMinScaled(void);
    double           GetOperatingRangeMaxScaled(void);
    bool             GetOutOfRangeLow          (void);
    bool             GetOutOfRangeHigh         (void);
    QString          GetShortUnits             (void);
    QString          GetLongUnits              (void);

  protected:
    virtual double   ScaleValue                (double Value) = 0;
    void             SetShortUnits             (const QString &Units);
    void             SetLongUnits              (const QString &Units);

  protected:
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
};

#endif // TORCSENSOR_H
