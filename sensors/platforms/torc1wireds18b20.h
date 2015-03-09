#ifndef TORC1WIREDS18B20_H
#define TORC1WIREDS18B20_H

// Qt
#include <QTimer>

// Torc
#include "torcqthread.h"
#include "torctemperaturesensor.h"
#include "torc1wirebus.h"

#define DS18B20NAME QString("ds18b20")

class Torc1WireReadThread;

class Torc1WireDS18B20 : public TorcTemperatureSensor
{
    Q_OBJECT

  public:
    Torc1WireDS18B20(const QVariantMap &Details);
    ~Torc1WireDS18B20();

    void   Read  (double Value, bool Valid);

  private:
    Torc1WireReadThread *m_readThread;
};


class Torc1WireReadThread : public TorcQThread
{
    Q_OBJECT

  public:
    Torc1WireReadThread(Torc1WireDS18B20 *Parent, const QString &Filename);
    void Start(void);
    void Finish(void);

  public slots:
    void Read(void);

  private:
    Torc1WireDS18B20  *m_parent;
    QTimer            *m_timer;
    QString            m_file;
};
#endif // TORC1WIREDS18B20_H
