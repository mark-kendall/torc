#ifndef TORC1WIREBUS_H
#define TORC1WIREBUS_H

// Qt
#include <QMap>
#include <QObject>

// Torc
#include "torcinput.h"
#include "torccentral.h"

#define ONE_WIRE_DIRECTORY QString("/sys/bus/w1/devices/")
#define ONE_WIRE_NAME      QString("wire1")

class Torc1WireBus : public TorcDeviceHandler
{
  public:
    Torc1WireBus();
    ~Torc1WireBus();

    static Torc1WireBus*        gTorc1WireBus;

    void                        Create  (const QVariantMap &Details);
    void                        Destroy (void);

  private:
    QHash<QString, TorcInput*>  m_inputs;
};

class Torc1WireDeviceFactory
{
  public:
    Torc1WireDeviceFactory();
    virtual ~Torc1WireDeviceFactory();

    static Torc1WireDeviceFactory*   GetTorc1WireDeviceFactory (void);
    Torc1WireDeviceFactory*          NextFactory               (void) const;
    virtual TorcInput*               Create                    (const QString &DeviceType, const QVariantMap &Details) = 0;

  protected:
    static Torc1WireDeviceFactory*   gTorc1WireDeviceFactory;
    Torc1WireDeviceFactory*          nextTorc1WireDeviceFactory;

  private:
    Q_DISABLE_COPY(Torc1WireDeviceFactory)
};

#endif // TORC1WIREBUS_H
