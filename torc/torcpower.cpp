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

        return nullptr;
    }
} TorcPowerFactoryNull;

/*! \class PowerFactory
 *
 *  \sa TorcPower
*/

TorcPowerFactory* TorcPowerFactory::gPowerFactory = nullptr;

TorcPowerFactory::TorcPowerFactory()
  : nextPowerFactory(gPowerFactory)
{
    gPowerFactory = this;
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
    TorcPower *power = nullptr;

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
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create power implementation"));

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
    TorcHTTPService(this, QStringLiteral("power"), QStringLiteral("power"), TorcPower::staticMetaObject, QStringLiteral("ShuttingDown,Suspending,Hibernating,Restarting,WokeUp,LowBattery,Refresh")),
    m_canShutdown(new TorcSetting(nullptr,  QStringLiteral("CanShutdown"),  QStringLiteral(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_canSuspend(new TorcSetting(nullptr,   QStringLiteral("CanSuspend"),   QStringLiteral(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_canHibernate(new TorcSetting(nullptr, QStringLiteral("CanHibernate"), QStringLiteral(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_canRestart(new TorcSetting(nullptr,   QStringLiteral("CanRestart"),   QStringLiteral(), TorcSetting::Bool, TorcSetting::None, QVariant((bool)false))),
    m_batteryLevel(TorcPower::UnknownPower),
    m_powerGroupItem(new TorcSettingGroup(gRootSetting, tr("Power"))),
    m_powerEnabled(new TorcSetting(m_powerGroupItem, QStringLiteral("EnablePower"),
                                   tr("Enable power management"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowShutdown(new TorcSetting(m_powerEnabled, QStringLiteral("AllowShutdown"),
                                   tr("Allow Torc to shutdown the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowSuspend(new TorcSetting(m_powerEnabled, QStringLiteral("AllowSuspend"),
                                   tr("Allow Torc to suspend the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowHibernate(new TorcSetting(m_powerEnabled, QStringLiteral("AllowHibernate"),
                                   tr("Allow Torc to hibernate the system"),
                                   TorcSetting::Bool, TorcSetting::Persistent | TorcSetting::Public, QVariant((bool)true))),
    m_allowRestart(new TorcSetting(m_powerEnabled, QStringLiteral("AllowRestart"),
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
    connect(m_allowShutdown,  static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ActiveChanged), this, &TorcPower::CanShutdownActiveChanged);
    connect(m_allowSuspend,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ActiveChanged), this, &TorcPower::CanSuspendActiveChanged);
    connect(m_allowHibernate, static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ActiveChanged), this, &TorcPower::CanHibernateActiveChanged);
    connect(m_allowRestart,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ActiveChanged), this, &TorcPower::CanRestartActiveChanged);

    connect(m_allowShutdown,  static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged),  this, &TorcPower::CanShutdownValueChanged);
    connect(m_allowSuspend,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged),  this, &TorcPower::CanSuspendValueChanged);
    connect(m_allowHibernate, static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged),  this, &TorcPower::CanHibernateValueChanged);
    connect(m_allowRestart,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged),  this, &TorcPower::CanRestartValueChanged);

    connect(m_canShutdown,    static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowShutdown,  &TorcSetting::SetActive);
    connect(m_canSuspend,     static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowSuspend,   &TorcSetting::SetActive);
    connect(m_canHibernate,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowHibernate, &TorcSetting::SetActive);
    connect(m_canRestart,     static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowRestart,   &TorcSetting::SetActive);

    connect(m_powerEnabled,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowShutdown,  &TorcSetting::SetActive);
    connect(m_powerEnabled,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowSuspend,   &TorcSetting::SetActive);
    connect(m_powerEnabled,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowHibernate, &TorcSetting::SetActive);
    connect(m_powerEnabled,   static_cast<void (TorcSetting::*)(bool)>(&TorcSetting::ValueChanged), m_allowRestart,   &TorcSetting::SetActive);

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
        caps += QStringLiteral("Shutdown ");
    if (m_canSuspend->GetValue().toBool())
        caps += QStringLiteral("Suspend ");
    if (m_canHibernate->GetValue().toBool())
        caps += QStringLiteral("Hibernate ");
    if (m_canRestart->GetValue().toBool())
        caps += QStringLiteral("Restart ");

    if (caps.isEmpty())
        caps = QStringLiteral("None");

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Power support: %1").arg(caps));
}

QString TorcPower::GetUIName(void)
{
    return tr("Power");
}

void TorcPower::BatteryUpdated(int Level)
{
    m_httpServiceLock.lockForWrite();

    if (m_lastBatteryLevel == Level)
    {
        m_httpServiceLock.unlock();
        return;
    }

    bool wasalreadylow = m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= BatteryLow;
    m_lastBatteryLevel = Level;

    if (m_lastBatteryLevel == ACPower)
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("On AC power"));
    else if (m_lastBatteryLevel == UnknownPower)
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Unknown power status"));
    else
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Battery level %1%").arg(m_lastBatteryLevel));


    bool lowbattery = !wasalreadylow && (m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= BatteryLow);
    int level = m_lastBatteryLevel;

    m_httpServiceLock.unlock();

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
    result.insert(QStringLiteral("canShutdown"),  GetCanShutdown());
    result.insert(QStringLiteral("canSuspend"),   GetCanSuspend());
    result.insert(QStringLiteral("canHibernate"), GetCanHibernate());
    result.insert(QStringLiteral("canRestart"),   GetCanRestart());
    result.insert(QStringLiteral("batteryLevel"), GetBatteryLevel());
    return result;
}

void TorcPower::ShuttingDown(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("System will shut down"));
    TorcLocalContext::NotifyEvent(Torc::ShuttingDown);
}

void TorcPower::Suspending(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("System will go to sleep"));
    TorcLocalContext::NotifyEvent(Torc::Suspending);
}

void TorcPower::Hibernating(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("System will hibernate"));
    TorcLocalContext::NotifyEvent(Torc::Hibernating);
}

void TorcPower::Restarting(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("System restarting"));
    TorcLocalContext::NotifyEvent(Torc::Restarting);
}

void TorcPower::WokeUp(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("System woke up"));
    TorcLocalContext::NotifyEvent(Torc::WokeUp);
}

void TorcPower::LowBattery(void)
{
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Sending low battery warning"));
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
        m_power(nullptr)
    {
    }

   ~TorcPowerObject()
    {
        Destroy();
    }

    void GetStrings(QVariantMap &Strings)
    {
        Strings.insert(QStringLiteral("Suspend"),          QCoreApplication::translate("TorcPower", "Suspend"));
        Strings.insert(QStringLiteral("Shutdown"),         QCoreApplication::translate("TorcPower", "Shutdown"));
        Strings.insert(QStringLiteral("Hibernate"),        QCoreApplication::translate("TorcPower", "Hibernate"));
        Strings.insert(QStringLiteral("Restart"),          QCoreApplication::translate("TorcPower", "Restart"));
        Strings.insert(QStringLiteral("ConfirmSuspend"),   QCoreApplication::translate("TorcPower", "Are you sure you want to suspend the device?"));
        Strings.insert(QStringLiteral("ConfirmShutdown"),  QCoreApplication::translate("TorcPower", "Are you sure you want to shutdown the device?"));
        Strings.insert(QStringLiteral("ConfirmHibernate"), QCoreApplication::translate("TorcPower", "Are you sure you want to hibernate the device?"));
        Strings.insert(QStringLiteral("ConfirmRestart"),   QCoreApplication::translate("TorcPower", "Are you sure you want to restart the device?"));
        Strings.insert(QStringLiteral("ACPowerTr"),        QCoreApplication::translate("TorcPower", "On AC Power"));
        Strings.insert(QStringLiteral("UnknownPowerTr"),   QCoreApplication::translate("TorcPower", "Unknown power status"));

        // string constants
        Strings.insert(QStringLiteral("ACPower"),          TorcPower::ACPower);
        Strings.insert(QStringLiteral("UnknownPower"),     TorcPower::UnknownPower);
    }

    void Create(void)
    {
        m_power = TorcPowerFactory::CreatePower();
    }

    void Destroy(void)
    {
        if (m_power)
            delete m_power;
        m_power = nullptr;
    }

  private:
    Q_DISABLE_COPY(TorcPowerObject)
    TorcPower *m_power;
} TorcPowerObject;
