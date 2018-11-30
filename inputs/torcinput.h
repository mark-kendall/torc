#ifndef TORCINPUT_H
#define TORCINPUT_H

// Qt
#include <QMutex>
#include <QObject>

// Torc
#include "http/torchttpservice.h"
#include "torcdevice.h"

#define INPUTS_DIRECTORY QString("inputs")

class TorcInput : public TorcDevice, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_PROPERTY(double   operatingRangeMin       READ GetOperatingRangeMin       CONSTANT)
    Q_PROPERTY(double   operatingRangeMax       READ GetOperatingRangeMax       CONSTANT)
    Q_PROPERTY(bool     outOfRangeLow           READ GetOutOfRangeLow           NOTIFY   OutOfRangeLowChanged)
    Q_PROPERTY(bool     outOfRangeHigh          READ GetOutOfRangeHigh          NOTIFY   OutOfRangeHighChanged)

  public:
    enum Type
    {
        Unknown = 0,
        Temperature,
        pH,
        Switch,
        PWM,
        Button,
        SystemStarted,
        MaxType
    };

    Q_ENUM(Type)

    static QString          TypeToString(TorcInput::Type Type);
    static TorcInput::Type  StringToType(const QString &Type);

  public:
    TorcInput(TorcInput::Type Type, double Value, double RangeMinimum, double RangeMaximum,
              const QString &ModelId,    const QVariantMap &Details);


    virtual TorcInput::Type  GetType           (void) = 0;
    virtual void             Start             (void) override;
    QString                  GetUIName         (void) override;

  protected:
    virtual ~TorcInput() = default;

  signals:
    void             OutOfRangeLowChanged      (bool Value);
    void             OutOfRangeHighChanged     (bool Value);

  public slots:
    // TorcHTTPService
    void             SubscriberDeleted         (QObject *Subscriber);

    void             SetValue                  (double Value) override;
    void             SetValid                  (bool Valid)   final;

    double           GetOperatingRangeMin      (void);
    double           GetOperatingRangeMax      (void);
    bool             GetOutOfRangeLow          (void);
    bool             GetOutOfRangeHigh         (void);

  protected:
    double           operatingRangeMin;
    double           operatingRangeMax;
    bool             outOfRangeLow;
    bool             outOfRangeHigh;
};

#endif // TORCINPUT_H
