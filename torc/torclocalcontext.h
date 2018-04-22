#ifndef TORCLOCALCONTEXT_H
#define TORCLOCALCONTEXT_H

// Qt
#include <QObject>
#include <QString>

// Torc
#include "torclocaldefs.h"
#include "torcsetting.h"
#include "torcobservable.h"
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
        // network
        NetworkAvailable = 2000,
        NetworkUnavailable,
        NetworkChanged,
        NetworkHostNamesChanged,
        // service discovery
        ServiceDiscovered = 3000,
        ServiceWentAway,
        // end of predefined
        MaxTorc = 60000,
        // plugins etc
        MaxUser = 65535
    };

    static QString ActionToString(enum Actions Action);
    static int     StringToAction(const QString &Action);
};

class TorcLocalContextPriv;
class QMutex;
class QReadWriteLock;

class TorcLocalContext : public QObject, public TorcObservable
{
    Q_OBJECT

  public:
    static qint16 Create      (TorcCommandLine* CommandLine, bool Init = true);
    static void   TearDown    (void);

    Q_INVOKABLE static void  NotifyEvent   (int Event);
    Q_INVOKABLE   QString    GetSetting    (const QString &Name, const QString &DefaultValue);
    Q_INVOKABLE   bool       GetSetting    (const QString &Name, const bool    &DefaultValue);
    Q_INVOKABLE   int        GetSetting    (const QString &Name, const int     &DefaultValue);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const QString &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const bool    &Value);
    Q_INVOKABLE   void       SetSetting    (const QString &Name, const int     &Value);
    Q_INVOKABLE   QString    GetPreference (const QString &Name, const QString &DefaultValue);
    Q_INVOKABLE   bool       GetPreference (const QString &Name, const bool    &DefaultValue);
    Q_INVOKABLE   int        GetPreference (const QString &Name, const int     &DefaultValue);
    Q_INVOKABLE   void       SetPreference (const QString &Name, const QString &Value);
    Q_INVOKABLE   void       SetPreference (const QString &Name, const bool    &Value);
    Q_INVOKABLE   void       SetPreference (const QString &Name, const int     &Value);
    Q_INVOKABLE   QObject*   GetUIObject   (void);
    Q_INVOKABLE   QString    GetUuid       (void) const;
    Q_INVOKABLE   TorcSetting* GetRootSetting (void);
    Q_INVOKABLE   qint64     GetStartTime  (void);
    Q_INVOKABLE   int        GetPriority   (void);

    QLocale                  GetLocale     (void);
    TorcLanguage*            GetLanguage   (void);
    void                     SetUIObject   (QObject* UI);
    void                     CloseDatabaseConnections (void);

  public slots:
    void                     RegisterQThread          (void);
    void                     DeregisterQThread        (void);

  private:
    explicit TorcLocalContext(TorcCommandLine* CommandLine);
   ~TorcLocalContext();

    bool          Init(void);

  private:
    TorcLocalContextPriv *m_priv;
};

extern TorcLocalContext *gLocalContext;
extern TorcSetting      *gRootSetting;

#endif // TORCLOCALCONTEXT_H
