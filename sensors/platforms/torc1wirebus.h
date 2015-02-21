#ifndef TORC1WIREBUS_H
#define TORC1WIREBUS_H

// Qt
#include <QMap>
#include <QObject>

// Torc
#include "torcsensor.h"
#include "torccentral.h"

#define ONE_WIRE_DIRECTORY QString("/sys/bus/w1/devices/")

class Torc1WireBus : public TorcDeviceHandler
{
  public:
    Torc1WireBus();
    ~Torc1WireBus();

    static Torc1WireBus*        gTorc1WireBus;

    void                        Create  (const QVariantMap &Details);
    void                        Destroy (void);

  private:
    QHash<QString, TorcSensor*> m_sensors;
};

#endif // TORC1WIREBUS_H
