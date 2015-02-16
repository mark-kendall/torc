#ifndef TORCSENSORS_H
#define TORCSENSORS_H

// Qt
#include <QList>
#include <QMutex>

// Torc
#include "torchttpservice.h"
#include "torcsensor.h"

class TorcSensors : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetSensorList",  "type=sensors")
    Q_PROPERTY(QVariantMap sensorList   READ GetSensorList() NOTIFY SensorsChanged())

  public:
    static TorcSensors* gSensors;

  public:
    TorcSensors();
    ~TorcSensors();

    void                Start                     (void);

  public slots:
    // TorcHTTPService
    void                SubscriberDeleted         (QObject *Subscriber);

    QVariantMap         GetSensorList             (void);

  signals:
    void                SensorsChanged            (void);

  public:
    void                AddSensor                 (TorcSensor *Sensor);
    void                RemoveSensor              (TorcSensor *Sensor);
    void                UpdateSensor              (const QString &Id, const QString &Name, const QString &Description);

  private:
    QList<TorcSensor*>  sensorList;
    QMutex             *m_lock;
};

#endif // TORCSENSORS_H
