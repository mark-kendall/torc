#ifndef TORCI2CBUS_H
#define TORCI2CBUS_H

// Qt
#include <QMap>
#include <QMutex>
#include <QVariant>

// Torc
#include "torccentral.h"

#define I2C QStringLiteral("i2c")

class TorcI2CDevice
{
  public:
    explicit TorcI2CDevice(int Address);
    virtual ~TorcI2CDevice()= default;

  protected:
    int  m_address;
    int  m_fd;

  private:
    Q_DISABLE_COPY(TorcI2CDevice)
};

class TorcI2CDeviceFactory
{
  public:
    TorcI2CDeviceFactory();
    virtual ~TorcI2CDeviceFactory() = default;

    static TorcI2CDeviceFactory*   GetTorcI2CDeviceFactory (void);
    TorcI2CDeviceFactory*          NextFactory             (void) const;
    virtual TorcI2CDevice*         Create                  (int Address, const QString &Name, const QVariantMap &Details) = 0;

  protected:
    static TorcI2CDeviceFactory*   gTorcI2CDeviceFactory;
    TorcI2CDeviceFactory*          nextTorcI2CDeviceFactory;

  private:
    Q_DISABLE_COPY(TorcI2CDeviceFactory)
};

class TorcI2CBus : public TorcDeviceHandler
{
  public:
    TorcI2CBus();

    static TorcI2CBus *gTorcI2CBus;

    void Create              (const QVariantMap &Details);
    void Destroy             (void);

  private:
    QMap<int,TorcI2CDevice*> m_devices;
};
#endif // TORCI2CBUS_H
