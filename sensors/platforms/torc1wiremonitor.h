#ifndef TORC1WIREMONITOR_H
#define TORC1WIREMONITOR_H

// Qt
#include <QMap>
#include <QTimer>
#include <QObject>
#include <QFileSystemWatcher>

// Torc
#include "../torcsensor.h"

class Torc1WireMonitor : public QObject
{
    Q_OBJECT

  public:
    Torc1WireMonitor();
    ~Torc1WireMonitor();

  public slots:
    void       Start               (void);
    void       DirectoryChanged    (const QString&);

  private:
    void       ScheduleRetry       (void);

  private:
    QTimer                     *m_timer;
    QFileSystemWatcher         *m_1wireDirectory;
    QHash<QString, TorcSensor*> m_sensors;
};

#endif // TORC1WIREMONITOR_H
