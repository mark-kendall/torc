#ifndef TORCLOCALCONTEXT_H
#define TORCLOCALCONTEXT_H

// Qt
#include <QObject>
#include <QString>
#include <QReadWriteLock>
#include <QMutex>

// Torc
#include "torclocaldefs.h"
#include "torcsetting.h"
#include "torcobservable.h"
#include "torcsqlitedb.h"
#include "torcadminthread.h"
#include "torccommandline.h"

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

class TorcAdminObject;
class TorcLanguage;

class Torc
{
    Q_GADGET
    Q_ENUMS(Actions)

  public:
    enum Actions
    {
        None = 0,
        Start,
        Stop,
        RestartTorc,
        UserChanged,
        TorcWillStop, // or restart etc
        // Power management
        Shutdown = 1000,
        Suspend,
        Hibernate,
        Restart,
        ShuttingDown,
        Suspending,
        Hibernating,
        Restarting,
        WokeUp,
        LowBattery,
        // these are issued directly to the power object
        // when a delayed event is issued
        ShutdownNow,
        SuspendNow,
        HibernateNow,
        RestartNow,
        // network
        NetworkAvailable = 2000,
        NetworkUnavailable,
        NetworkChanged,
        NetworkHostNamesChanged,
        // service discovery
        ServiceDiscovered = 3000,
        ServiceWentAway,
        // system time
        SystemTimeChanged = 4000,
        // end of predefined
        MaxTorc = 60000,
        // plugins etc
        MaxUser = 65535
    };

    static QString ActionToString(enum Actions Action);
    static int     StringToAction(const QString &Action);
};

class TorcLocalContext : public QObject, public TorcObservable
{
    Q_OBJECT

  public:
    static qint16            Create              (TorcCommandLine* CommandLine, bool Init = true);
    static void              TearDown            (void);

    static void              NotifyEvent         (int Event);
    QString                  GetSetting          (const QString &Name, const QString &DefaultValue);
    bool                     GetSetting          (const QString &Name, const bool    &DefaultValue);
    int                      GetSetting          (const QString &Name, const int     &DefaultValue);
    void                     SetSetting          (const QString &Name, const QString &Value);
    void                     SetSetting          (const QString &Name, const bool    &Value);
    void                     SetSetting          (const QString &Name, const int     &Value);
    QString                  GetUuid             (void) const;
    TorcSetting*             GetRootSetting      (void);
    qint64                   GetStartTime        (void);
    int                      GetPriority         (void);

    QLocale                  GetLocale           (void);
    TorcLanguage*            GetLanguage         (void);
    void                     CloseDatabaseConnections (void);
    bool                     QueueShutdownEvent  (int Event);
    void                     SetShutdownDelay    (uint Delay);
    uint                     GetShutdownDelay    (void);

  public slots:
    void                     RegisterQThread     (void);
    void                     DeregisterQThread   (void);
    bool                     event               (QEvent *Event) override;
    void                     ShutdownTimeout     (void);

  private:
    explicit TorcLocalContext(TorcCommandLine* CommandLine);
   ~TorcLocalContext();
    Q_DISABLE_COPY(TorcLocalContext)

    bool                     Init                (void);
    QString                  GetDBSetting        (const QString &Name, const QString &DefaultValue);
    void                     SetDBSetting        (const QString &Name, const QString &Value);
    bool                     HandleShutdown      (int Event);

  private:
    TorcSQLiteDB            *m_sqliteDB;
    QString                  m_dbName;
    QMap<QString,QString>    m_localSettings;
    QReadWriteLock           m_localSettingsLock;
    TorcAdminThread         *m_adminThread;
    TorcLanguage            *m_language;
    QString                  m_uuid;
    uint                     m_shutdownDelay;
    int                      m_shutdownEvent;
};

extern TorcLocalContext    *gLocalContext;
extern TorcSetting         *gRootSetting;

#endif // TORCLOCALCONTEXT_H
