/* Class TorcPower
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QtGlobal>
#include <QCoreApplication>

// Torc
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torclanguage.h"
#include "torcpower.h"

/*! \class TorcPowerNull
 *  \brief A dummy/default power implementation.
*/

class TorcPowerNull : public TorcPower
{
  public:
    TorcPowerNull() : TorcPower() { Debug(); }
   ~TorcPowerNull()               { }
    bool DoShutdown        (void) { return false; }
    bool DoSuspend         (void) { return false; }
    bool DoHibernate       (void) { return false; }
    bool DoRestart         (void) { return false; }
};

class TorcPowerFactoryNull : public TorcPowerFactory
{
    void Score(int &Score)
    {
        if (Score <= 1)
            Score = 1;
    }

    TorcPower* Create(int Score)
    {
        if (Score <= 1)
            return new TorcPowerNull();

        return NULL;
    }
} TorcPowerFactoryNull;

/*! \class PowerFactory
 *
 *  \sa TorcPower
*/

TorcPowerFactory* TorcPowerFactory::gPowerFactory = NULL;

TorcPowerFactory::TorcPowerFactory()
  : nextPowerFactory(gPowerFactory)
{
    gPowerFactory = this;
}

TorcPowerFactory::~TorcPowerFactory()
{
}

TorcPowerFactory* TorcPowerFactory::GetTorcPowerFactory(void)
{
    return gPowerFactory;
}

TorcPowerFactory* TorcPowerFactory::NextPowerFactory(void) const
{
    return nextPowerFactory;
}

TorcPower* TorcPowerFactory::CreatePower()
{
    TorcPower *power = NULL;

    int score = 0;
    TorcPowerFactory* factory = TorcPowerFactory::GetTorcPowerFactory();
    for ( ; factory; factory = factory->NextPowerFactory())
        (void)factory->Score(score);

    factory = TorcPowerFactory::GetTorcPowerFactory();
    for ( ; factory; factory = factory->NextPowerFactory())
    {
        power = factory->Create(score);
        if (power)
            break;
    }

    if (!power)
        LOG(VB_GENERAL, LOG_ERR, "Failed to create power implementation");

    return power;
}

/*! \class TorcPower
 *  \brief A generic power status class.
 *
 * TorcPower uses underlying platform implementations to monitor the system's power status
 * and emits appropriate notifications (via TorcLocalContext) when the status changes.
 * Additional implementations can be added by sub-classing PowerFactory and TorcPowerPriv.
 *
 * The current power status (battery charge level or on mains power) can be queried directly via
 * GetBatteryLevel and the system's ability to Suspend/Shutdown etc can be queried via CanSuspend, CanShutdown,
 * CanHibernate and CanRestart.
 *
 * A singleton power object is created by TorcPowerObject from within the administration thread
 * - though TorcPower may be accessed from multiple threads and hence implementations must
 * guard against concurrent access if necessary.
 *
 * \note Locking in TorcPower is complicated by the public TorcSetting's that are HTTP services in their own right.
 *       TorcSetting is however extensively locked for this reason.
 *
 * \sa TorcPowerFactory
 * \sa TorcPowerObject
*/

TorcPower::TorcPower()
  : QObject(),
    TorcHTTPService(this, "power", "power", TorcPower::staticMetaObject, "ShuttingDown,Suspending,Hibernating,Restarting,WokeUp,LowBattery,Refresh"),
    m_canShutdown(new TorcSetting(NULL,  QString("CanShutdown"),  QString(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_canSuspend(new TorcSetting(NULL,   QString("CanSuspend"),   QString(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_canHibernate(new TorcSetting(NULL, QString("CanHibernate"), QString(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_canRestart(new TorcSetting(NULL,   QString("CanRestart"),   QString(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_batteryLevel(TorcPower::UnknownPower),
    m_powerGroupItem(new TorcSettingGroup(gRootSetting, tr("Power"))),
    m_powerEnabled(new TorcSetting(m_powerGroupItem, QString("EnablePower"),
                                   tr("Enable power management"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowShutdown(new TorcSetting(m_powerEnabled, QString("AllowShutdown"),
                                   tr("Allow Torc to shutdown the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowSuspend(new TorcSetting(m_powerEnabled, QString("AllowSuspend"),
                                   tr("Allow Torc to suspend the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowHibernate(new TorcSetting(m_powerEnabled, QString("AllowHibernate"),
                                   tr("Allow Torc to hibernate the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowRestart(new TorcSetting(m_powerEnabled, QString("AllowRestart"),
                                   tr("Allow Torc to restart the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_lastBatteryLevel(UnknownPower),
    canShutdown(false),
    canSuspend(false),
    canHibernate(false),
    canRestart(false),
    batteryLevel(UnknownPower)
{
    m_powerEnabled->SetActive(true);
    // 'allow' depends on both underlying platform capabilities and top level setting
    m_allowShutdown->SetActiveThreshold(2);
    m_allowSuspend->SetActiveThreshold(2);
    m_allowHibernate->SetActiveThreshold(2);
    m_allowRestart->SetActiveThreshold(2);

    if (m_canShutdown->GetValue().toBool())
        m_allowShutdown->SetActive(true);
    if (m_canSuspend->GetValue().toBool())
        m_allowSuspend->SetActive(true);
    if (m_canHibernate->GetValue().toBool())
        m_allowHibernate->SetActive(true);
    if (m_canRestart->GetValue().toBool())
        m_allowRestart->SetActive(true);

    if (m_powerEnabled->GetValue().toBool())
    {
        m_allowShutdown->SetActive(true);
        m_allowSuspend->SetActive(true);
        m_allowHibernate->SetActive(true);
        m_allowRestart->SetActive(true);
    }

    // listen for changes
    connect(m_allowShutdown, SIGNAL(ActiveChanged(bool)), this, SLOT(CanShutdownActiveChanged(bool)));
    connect(m_allowShutdown, SIGNAL(ValueChanged(bool)),  this, SLOT(CanShutdownValueChanged(bool)));
    connect(m_allowSuspend,  SIGNAL(ActiveChanged(bool)), this, SLOT(CanSuspendActiveChanged(bool)));
    connect(m_allowSuspend,  SIGNAL(ValueChanged(bool)),  this, SLOT(CanSuspendValueChanged(bool)));
    connect(m_allowHibernate,SIGNAL(ActiveChanged(bool)), this, SLOT(CanHibernateActiveChanged(bool)));
    connect(m_allowHibernate,SIGNAL(ValueChanged(bool)),  this, SLOT(CanHibernateValueChanged(bool)));
    connect(m_allowRestart,  SIGNAL(ActiveChanged(bool)), this, SLOT(CanRestartActiveChanged(bool)));
    connect(m_allowRestart,  SIGNAL(ValueChanged(bool)),  this, SLOT(CanRestartValueChanged(bool)));

    connect(m_canShutdown,  SIGNAL(ValueChanged(bool)), m_allowShutdown,  SLOT(SetActive(bool)));
    connect(m_canSuspend,   SIGNAL(ValueChanged(bool)), m_allowSuspend,   SLOT(SetActive(bool)));
    connect(m_canHibernate, SIGNAL(ValueChanged(bool)), m_allowHibernate, SLOT(SetActive(bool)));
    connect(m_canRestart,   SIGNAL(ValueChanged(bool)), m_allowRestart,   SLOT(SetActive(bool)));

    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowShutdown,  SLOT(SetActive(bool)));
    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowSuspend,   SLOT(SetActive(bool)));
    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowHibernate, SLOT(SetActive(bool)));
    connect(m_powerEnabled,         SIGNAL(ValueChanged(bool)), m_allowRestart,   SLOT(SetActive(bool)));

    // listen for delayed shutdown events
    gLocalContext->AddObserver(this);
}

TorcPower::~TorcPower()
{
    gLocalContext->RemoveObserver(this);

    if (m_canShutdown)
        m_canShutdown->DownRef();

    if (m_canSuspend)
        m_canSuspend->DownRef();

    if (m_canHibernate)
        m_canHibernate->DownRef();

    if (m_canRestart)
        m_canRestart->DownRef();

    if (m_allowShutdown)
    {
        m_allowShutdown->Remove();
        m_allowShutdown->DownRef();
    }

    if (m_allowRestart)
    {
        m_allowRestart->Remove();
        m_allowRestart->DownRef();
    }

    if (m_allowHibernate)
    {
        m_allowHibernate->Remove();
        m_allowHibernate->DownRef();
    }

    if (m_allowSuspend)
    {
        m_allowSuspend->Remove();
        m_allowSuspend->DownRef();
    }

    if (m_powerEnabled)
    {
        m_powerEnabled->Remove();
        m_powerEnabled->DownRef();
    }

    if (m_powerGroupItem)
    {
        m_powerGroupItem->Remove();
        m_powerGroupItem->DownRef();
    }
}

void TorcPower::Debug(void)
{
    QString caps;

    if (m_canShutdown->GetValue().toBool())
        caps += "Shutdown ";
    if (m_canSuspend->GetValue().toBool())
        caps += "Suspend ";
    if (m_canHibernate->GetValue().toBool())
        caps += "Hibernate ";
    if (m_canRestart->GetValue().toBool())
        caps += "Restart ";

    if (caps.isEmpty())
        caps = "None";

    LOG(VB_GENERAL, LOG_INFO, QString("Power support: %1").arg(caps));
}

QString TorcPower::GetUIName(void)
{
    return tr("Power");
}

void TorcPower::BatteryUpdated(int Level)
{
    bool lowbattery = false;
    int  level      = m_lastBatteryLevel;

    {
        QWriteLocker locker(&m_httpServiceLock);

        if (m_lastBatteryLevel == Level)
            return;

        bool wasalreadylow = m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= BatteryLow;
        m_lastBatteryLevel = Level;

        if (m_lastBatteryLevel == ACPower)
            LOG(VB_GENERAL, LOG_INFO, "On AC power");
        else if (m_lastBatteryLevel == UnknownPower)
            LOG(VB_GENERAL, LOG_INFO, "Unknown power status");
        else
            LOG(VB_GENERAL, LOG_INFO, QString("Battery level %1%").arg(m_lastBatteryLevel));


        lowbattery = !wasalreadylow && (m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= BatteryLow);
        level = m_lastBatteryLevel;
    }

    if (lowbattery)
        LowBattery();
    emit BatteryLevelChanged(level);
}

bool TorcPower::event(QEvent *Event)
{
    TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
    if (torcevent)
    {
        int event = torcevent->GetEvent();
        switch (event)
        {
            case Torc::ShutdownNow:
                return DoShutdown();
            case Torc::RestartNow:
                return DoRestart();
            case Torc::SuspendNow:
                return DoSuspend();
            case Torc::HibernateNow:
                return DoHibernate();
            default: break;
        }
    }

    return QObject::event(Event);
}

bool TorcPower::Shutdown(void)
{
    m_httpServiceLock.lockForRead();
    bool shutdown = m_allowShutdown->GetValue().toBool() && m_allowShutdown->GetIsActive();
    m_httpServiceLock.unlock();

    if (shutdown)
        return gLocalContext->QueueShutdownEvent(Torc::Shutdown) ? true : DoShutdown();
    return false;
}

bool TorcPower::Suspend(void)
{
    m_httpServiceLock.lockForRead();
    bool suspend = m_allowSuspend->GetValue().toBool() && m_allowSuspend->GetIsActive();
    m_httpServiceLock.unlock();

    if (suspend)
        return gLocalContext->QueueShutdownEvent(Torc::Suspend) ? true : DoSuspend();
    return false;
}

bool TorcPower::Hibernate(void)
{
    m_httpServiceLock.lockForRead();
    bool hibernate = m_allowHibernate->GetValue().toBool() && m_allowHibernate->GetIsActive();
    m_httpServiceLock.unlock();

    if (hibernate)
        return gLocalContext->QueueShutdownEvent(Torc::Hibernate) ? true : DoHibernate();
    return false;
}

bool TorcPower::Restart(void)
{
    m_httpServiceLock.lockForRead();
    bool restart = m_allowRestart->GetValue().toBool() && m_allowRestart->GetIsActive();
    m_httpServiceLock.unlock();

    if (restart)
        return gLocalContext->QueueShutdownEvent(Torc::Restart) ? true : DoRestart();
    return false;
}

void TorcPower::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

bool TorcPower::GetCanShutdown(void)
{
    m_httpServiceLock.lockForRead();
    bool result = m_allowShutdown->GetValue().toBool() && m_allowShutdown->GetIsActive();
    m_httpServiceLock.unlock();
    return result;
}

bool TorcPower::GetCanSuspend(void)
{
    m_httpServiceLock.lockForRead();
    bool result = m_allowSuspend->GetValue().toBool() && m_allowSuspend->GetIsActive();
    m_httpServiceLock.unlock();
    return result;
}

bool TorcPower::GetCanHibernate(void)
{
    m_httpServiceLock.lockForRead();
    bool result = m_allowHibernate->GetValue().toBool() && m_allowHibernate->GetIsActive();
    m_httpServiceLock.unlock();
    return result;
}

bool TorcPower::GetCanRestart(void)
{
    m_httpServiceLock.lockForRead();
    bool result = m_allowRestart->GetValue().toBool() && m_allowRestart->GetIsActive();
    m_httpServiceLock.unlock();
    return result;
}

int TorcPower::GetBatteryLevel(void)
{
    m_httpServiceLock.lockForRead();
    int result = m_batteryLevel;
    m_httpServiceLock.unlock();
    return result;
}

void TorcPower::CanShutdownActiveChanged(bool Active)
{
    m_httpServiceLock.lockForRead();
    bool changed = m_allowShutdown->GetValue().toBool() && Active;
    m_httpServiceLock.unlock();
    emit CanShutdownChanged(changed);
}

void TorcPower::CanShutdownValueChanged(bool Value)
{
    m_httpServiceLock.lockForRead();
    bool changed = Value && m_allowShutdown->GetIsActive();
    m_httpServiceLock.unlock();
    emit CanShutdownChanged(changed);
}

void TorcPower::CanSuspendActiveChanged(bool Active)
{
    m_httpServiceLock.lockForRead();
    bool changed = m_allowSuspend->GetValue().toBool() && Active;
    m_httpServiceLock.unlock();
    emit CanSuspendChanged(changed);
}

void TorcPower::CanSuspendValueChanged(bool Value)
{
    m_httpServiceLock.lockForRead();
    bool changed = Value && m_allowSuspend->GetIsActive();
    m_httpServiceLock.unlock();
    emit CanSuspendChanged(changed);
}

void TorcPower::CanHibernateActiveChanged(bool Active)
{
    m_httpServiceLock.lockForRead();
    bool changed = m_allowHibernate->GetValue().toBool() && Active;
    m_httpServiceLock.unlock();
    emit CanHibernateChanged(changed);
}

void TorcPower::CanHibernateValueChanged(bool Value)
{
    m_httpServiceLock.lockForRead();
    bool changed = Value && m_allowHibernate->GetIsActive();
    m_httpServiceLock.unlock();
    emit CanHibernateChanged(changed);
}

void TorcPower::CanRestartActiveChanged(bool Active)
{
    m_httpServiceLock.lockForRead();
    bool changed = m_allowHibernate->GetValue().toBool() && Active;
    m_httpServiceLock.unlock();
    emit CanRestartChanged(changed);
}

void TorcPower::CanRestartValueChanged(bool Value)
{
    m_httpServiceLock.lockForRead();
    bool changed = Value && m_allowHibernate->GetIsActive();
    m_httpServiceLock.unlock();
    emit CanRestartChanged(changed);
}

QVariantMap TorcPower::GetPowerStatus(void)
{
    QVariantMap result;
    result.insert("canShutdown",  GetCanShutdown());
    result.insert("canSuspend",   GetCanSuspend());
    result.insert("canHibernate", GetCanHibernate());
    result.insert("canRestart",   GetCanRestart());
    result.insert("batteryLevel", GetBatteryLevel());
    return result;
}

void TorcPower::ShuttingDown(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System will shut down");
    TorcLocalContext::NotifyEvent(Torc::ShuttingDown);
}

void TorcPower::Suspending(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System will go to sleep");
    TorcLocalContext::NotifyEvent(Torc::Suspending);
}

void TorcPower::Hibernating(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System will hibernate");
    TorcLocalContext::NotifyEvent(Torc::Hibernating);
}

void TorcPower::Restarting(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System restarting");
    TorcLocalContext::NotifyEvent(Torc::Restarting);
}

void TorcPower::WokeUp(void)
{
    LOG(VB_GENERAL, LOG_INFO, "System woke up");
    TorcLocalContext::NotifyEvent(Torc::WokeUp);
}

void TorcPower::LowBattery(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Sending low battery warning");
    TorcLocalContext::NotifyEvent(Torc::LowBattery);
}

/*! \class TorcPowerObject
 *  \brief A static class used to create the TorcPower singleton in the admin thread.
*/
static class TorcPowerObject : public TorcAdminObject, public TorcStringFactory
{
    Q_DECLARE_TR_FUNCTIONS(TorcPowerObject)

  public:
    TorcPowerObject()
      : TorcAdminObject(TORC_ADMIN_MED_PRIORITY),
        m_power(NULL)
    {
    }

   ~TorcPowerObject()
    {
        Destroy();
    }

    void GetStrings(QVariantMap &Strings)
    {
        Strings.insert("Suspend",          QCoreApplication::translate("TorcPower", "Suspend"));
        Strings.insert("Shutdown",         QCoreApplication::translate("TorcPower", "Shutdown"));
        Strings.insert("Hibernate",        QCoreApplication::translate("TorcPower", "Hibernate"));
        Strings.insert("Restart",          QCoreApplication::translate("TorcPower", "Restart"));
        Strings.insert("ConfirmSuspend",   QCoreApplication::translate("TorcPower", "Are you sure you want to suspend the device?"));
        Strings.insert("ConfirmShutdown",  QCoreApplication::translate("TorcPower", "Are you sure you want to shutdown the device?"));
        Strings.insert("ConfirmHibernate", QCoreApplication::translate("TorcPower", "Are you sure you want to hibernate the device?"));
        Strings.insert("ConfirmRestart",   QCoreApplication::translate("TorcPower", "Are you sure you want to restart the device?"));
        Strings.insert("ACPowerTr",        QCoreApplication::translate("TorcPower", "On AC Power"));
        Strings.insert("UnknownPowerTr",   QCoreApplication::translate("TorcPower", "Unknown power status"));

        // string constants
        Strings.insert("ACPower",          TorcPower::ACPower);
        Strings.insert("UnknownPower",     TorcPower::UnknownPower);
    }

    void Create(void)
    {
        m_power = TorcPowerFactory::CreatePower();
    }

    void Destroy(void)
    {
        if (m_power)
            delete m_power;
        m_power = NULL;
    }

  private:
    Q_DISABLE_COPY(TorcPowerObject)
    TorcPower *m_power;
} TorcPowerObject;
