#ifndef TORCSENSORS_H
#define TORCSENSORS_H

// Qt
#include <QList>
#include <QMutex>

// Torc
#include "torchttpservice.h"
#include "torcsensor.h"
#include "torccentral.h"

class TorcSensors : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetSensorList",  "type=sensors")
    Q_PROPERTY(QVariantMap sensorList   READ GetSensorList() NOTIFY SensorsChanged());
    Q_PROPERTY(QStringList sensorTypes  READ GetSensorTypes() CONSTANT);

  public:
    static TorcSensors* gSensors;

  public:
    TorcSensors();
    ~TorcSensors();

    void                Graph                     (QByteArray* Data);
    QString             GetUIName                 (void);

  public slots:
    // TorcHTTPService
    void                SubscriberDeleted         (QObject *Subscriber);

    QVariantMap         GetSensorList             (void);
    QStringList         GetSensorTypes            (void);

  signals:
    void                SensorsChanged            (void);

  public:
    void                AddSensor                 (TorcSensor *Sensor);
    void                RemoveSensor              (TorcSensor *Sensor);

  private:
    QList<TorcSensor*>  sensorList;
    QStringList         sensorTypes;
    QMutex             *m_lock;
};

#endif // TORCSENSORS_H
