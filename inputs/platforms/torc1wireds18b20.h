#ifndef TORC1WIREDS18B20_H
#define TORC1WIREDS18B20_H

// Qt
#include <QFile>
#include <QTimer>

// Torc
#include "torcqthread.h"
#include "torctemperatureinput.h"
#include "torc1wirebus.h"

#define DS18B20NAME QStringLiteral("ds18b20")

class Torc1WireDS18B20;

class Torc1WireReader final : public QObject
{
    Q_OBJECT
  public:
    Torc1WireReader(const QString &DeviceName);
   ~Torc1WireReader() = default;

  public slots:
    void Read(void);

  signals:
    void NewTemperature(double Value, double Valid);

  private:
    Q_DISABLE_COPY(Torc1WireReader)
    QTimer m_timer;
    QFile  m_file;
};

class Torc1WireReadThread final : public TorcQThread
{
    Q_OBJECT

  public:
    Torc1WireReadThread(Torc1WireDS18B20 *Parent, const QString &DeviceName);
    void Start(void) override;
    void Finish(void) override;

  private:
    Torc1WireDS18B20  *m_parent;
    Torc1WireReader   *m_reader;
    QString            m_deviceName;

  private:
    Q_DISABLE_COPY(Torc1WireReadThread)
};

class Torc1WireDS18B20 final : public TorcTemperatureInput
{
    Q_OBJECT

  public:
    explicit Torc1WireDS18B20(const QVariantMap &Details);
    ~Torc1WireDS18B20();

    QStringList GetDescription (void) override;

  public slots:
    void        Read           (double Value, bool Valid);

  private:
    QString              m_deviceId;
    Torc1WireReadThread  m_readThread;
};

#endif // TORC1WIREDS18B20_H
