#ifndef TORCPOWER_H
#define TORCPOWER_H

// Qt
#include <QObject>
#include <QMutex>

// Torc
#include "torcsetting.h"
#include "torchttpservice.h"

class TorcPower : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",   "1.0.0")
    Q_CLASSINFO("Shutdown",  "methods=PUT")
    Q_CLASSINFO("Suspend",   "methods=PUT")
    Q_CLASSINFO("Hibernate", "methods=PUT")
    Q_CLASSINFO("Restart",   "methods=PUT")
    Q_PROPERTY(bool canShutdown  READ GetCanShutdown  NOTIFY CanShutdownChanged)
    Q_PROPERTY(bool canSuspend   READ GetCanSuspend   NOTIFY CanSuspendChanged)
    Q_PROPERTY(bool canHibernate READ GetCanHibernate NOTIFY CanHibernateChanged)
    Q_PROPERTY(bool canRestart   READ GetCanRestart   NOTIFY CanRestartChanged)
    Q_PROPERTY(int  batteryLevel READ GetBatteryLevel NOTIFY BatteryLevelChanged)

    Q_ENUMS(PowerLevels)

  public:
    enum PowerLevels
    {
        ACPower       = -1,
        BatteryEmpty  = 0,
        BatteryLow    = 10,
        BatteryFull   = 100,
        UnknownPower  = 101
    };

  public:
    static void    Create(void);
    static void    TearDown(void);

  public:
    virtual ~TorcPower();
    QString GetUIName    (void) override;
    bool event           (QEvent *Event) override;

  signals:
    void CanShutdownChanged  (bool CanShutdown);
    void CanSuspendChanged   (bool CanSuspend);
    void CanHibernateChanged (bool CanHibernate);
    void CanRestartChanged   (bool CanRestart);
    void BatteryLevelChanged (int  BatteryLevel);

  public slots:
    void SubscriberDeleted (QObject *Subscriber);

    bool GetCanShutdown  (void);
    bool GetCanSuspend   (void);
    bool GetCanHibernate (void);
    bool GetCanRestart   (void);
    int  GetBatteryLevel (void);
    QVariantMap GetPowerStatus (void);

    bool Shutdown        (void);
    bool Suspend         (void);
    bool Hibernate       (void);
    bool Restart         (void);

    void ShuttingDown    (void);
    void Suspending      (void);
    void Hibernating     (void);
    void Restarting      (void);
    void WokeUp          (void);
    void LowBattery      (void);

  protected slots:
    void CanShutdownActiveChanged  (bool Active);
    void CanSuspendActiveChanged   (bool Active);
    void CanHibernateActiveChanged (bool Active);
    void CanRestartActiveChanged   (bool Active);
    void CanShutdownValueChanged   (bool Value);
    void CanSuspendValueChanged    (bool Value);
    void CanHibernateValueChanged  (bool Value);
    void CanRestartValueChanged    (bool Value);

  protected:
    TorcPower();
    void                  BatteryUpdated  (int Level);
    void                  Debug           (void);
    virtual bool          DoShutdown      (void) = 0;
    virtual bool          DoSuspend       (void) = 0;
    virtual bool          DoHibernate     (void) = 0;
    virtual bool          DoRestart       (void) = 0;

    TorcSetting          *m_canShutdown;
    TorcSetting          *m_canSuspend;
    TorcSetting          *m_canHibernate;
    TorcSetting          *m_canRestart;
    int                   m_batteryLevel;

  private:
    TorcSettingGroup     *m_powerGroupItem;
    TorcSetting          *m_powerEnabled;
    TorcSetting          *m_allowShutdown;
    TorcSetting          *m_allowSuspend;
    TorcSetting          *m_allowHibernate;
    TorcSetting          *m_allowRestart;
    int                   m_lastBatteryLevel;

    bool                  canShutdown;  // dummy
    bool                  canSuspend;   // dummy
    bool                  canHibernate; // dummy
    bool                  canRestart;   // dummy
    int                   batteryLevel; // dummy

  private:
    Q_DISABLE_COPY(TorcPower)
};

class TorcPowerFactory
{
  public:
    TorcPowerFactory();
    virtual ~TorcPowerFactory();

    static TorcPower*          CreatePower         (void);
    static TorcPowerFactory*   GetTorcPowerFactory (void);
    TorcPowerFactory*          NextPowerFactory    (void) const;
    virtual void               Score               (int &Score) = 0;
    virtual TorcPower*         Create              (int Score) = 0;

  protected:
    static TorcPowerFactory*   gPowerFactory;
    TorcPowerFactory*          nextPowerFactory;

  private:
    Q_DISABLE_COPY(TorcPowerFactory)
};

#endif // TORCPOWER_H
