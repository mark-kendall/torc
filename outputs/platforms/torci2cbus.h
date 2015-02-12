#ifndef TORCI2CBUS_H
#define TORCI2CBUS_H

// Qt
#include <QMap>
#include <QMutex>
#include <QVariant>

// Torc
#include "torccentral.h"

class TorcI2CDevice
{
  public:
    TorcI2CDevice(int Address);
    virtual ~TorcI2CDevice();

  protected:
    int m_address;
    int m_fd;
};

class TorcI2CDeviceFactory
{
  public:
    TorcI2CDeviceFactory();
    virtual ~TorcI2CDeviceFactory();

    static TorcI2CDeviceFactory*   GetTorcI2CDeviceFactory (void);
    TorcI2CDeviceFactory*          NextFactory             (void) const;
    virtual TorcI2CDevice*         Create                  (int Address, const QString &Name, const QVariantMap &Details) = 0;

  protected:
    static TorcI2CDeviceFactory*   gTorcI2CDeviceFactory;
    TorcI2CDeviceFactory*          nextTorcI2CDeviceFactory;
};

class TorcI2CBus : public TorcDeviceHandler
{
  public:
    TorcI2CBus();
   ~TorcI2CBus();

    static TorcI2CBus *gTorcI2CBus;

    void Create              (const QVariantMap &Details);
    void Destroy             (void);

  private:
    QMutex                  *m_lock;
    QMap<int,TorcI2CDevice*> m_devices;
};
#endif // TORCI2CBUS_H
