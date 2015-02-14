#ifndef TORCNETWORKSENSORS_H
#define TORCNETWORKSENSORS_H

// Qt
#include <QMutex>

// Torc
#include "torccentral.h"
#include "torcsensor.h"

#define NETWORK_SENSORS_STRING QString("Network")

class TorcNetworkSensors : public TorcDeviceHandler
{
  public:
    TorcNetworkSensors();
   ~TorcNetworkSensors();

    static TorcNetworkSensors*  gNetworkSensors;

    void                        Create      (const QVariantMap &Details);
    void                        Destroy     (void);

  private:
    QMap<QString,TorcSensor*>   m_sensors;
};

#endif // TORCNETWORKSENSORS_H
