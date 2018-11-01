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
 * \sa PowerFactory
 * \sa TorcPowerObject
 *
 * \todo Make HTTP acccess thread safe.
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

    m_canShutdown    = NULL;
    m_canSuspend     = NULL;
    m_canHibernate   = NULL;
    m_canRestart     = NULL;
    m_allowShutdown  = NULL;
    m_allowRestart   = NULL;
    m_allowHibernate = NULL;
    m_allowSuspend   = NULL;
    m_powerEnabled   = NULL;
    m_powerGroupItem = NULL;
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

    if (!wasalreadylow && (m_lastBatteryLevel >= 0 && m_lastBatteryLevel <= BatteryLow))
        LowBattery();

    emit BatteryLevelChanged(m_lastBatteryLevel);
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
    if (m_allowShutdown->GetValue().toBool() && m_allowShutdown->GetIsActive())
    {
        if (gLocalContext->QueueShutdownEvent(Torc::Shutdown))
            return true;
        return DoShutdown();
    }
    return false;
}

bool TorcPower::Suspend(void)
{
    if (m_allowSuspend->GetValue().toBool() && m_allowSuspend->GetIsActive())
    {
        if (gLocalContext->QueueShutdownEvent(Torc::Suspend))
            return true;
        return DoSuspend();
    }
    return false;
}

bool TorcPower::Hibernate(void)
{
    if (m_allowHibernate->GetValue().toBool() && m_allowHibernate->GetIsActive())
    {
        if (gLocalContext->QueueShutdownEvent(Torc::Hibernate))
            return true;
        return DoHibernate();
    }
    return false;
}

bool TorcPower::Restart(void)
{
    if (m_allowRestart->GetValue().toBool() && m_allowRestart->GetIsActive())
    {
        if (gLocalContext->QueueShutdownEvent(Torc::Restart))
            return true;
        return DoRestart();
    }
    return false;
}

void TorcPower::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

bool TorcPower::GetCanShutdown(void)
{
    return m_allowShutdown->GetValue().toBool() && m_allowShutdown->GetIsActive();
}

bool TorcPower::GetCanSuspend(void)
{
    return m_allowSuspend->GetValue().toBool() && m_allowSuspend->GetIsActive();
}

bool TorcPower::GetCanHibernate(void)
{
    return m_allowHibernate->GetValue().toBool() && m_allowHibernate->GetIsActive();
}

bool TorcPower::GetCanRestart(void)
{
    return m_allowRestart->GetValue().toBool() && m_allowRestart->GetIsActive();
}

int TorcPower::GetBatteryLevel(void)
{
    return m_batteryLevel;
}

void TorcPower::CanShutdownActiveChanged(bool Active)
{
    emit CanShutdownChanged(m_allowShutdown->GetValue().toBool() && Active);
}

void TorcPower::CanShutdownValueChanged(bool Value)
{
    emit CanShutdownChanged(Value && m_allowShutdown->GetIsActive());
}

void TorcPower::CanSuspendActiveChanged(bool Active)
{
    emit CanSuspendChanged(m_allowSuspend->GetValue().toBool() && Active);
}

void TorcPower::CanSuspendValueChanged(bool Value)
{
    emit CanSuspendChanged(Value && m_allowSuspend->GetIsActive());
}

void TorcPower::CanHibernateActiveChanged(bool Active)
{
    emit CanHibernateChanged(m_allowHibernate->GetValue().toBool() && Active);
}

void TorcPower::CanHibernateValueChanged(bool Value)
{
    emit CanHibernateChanged(Value && m_allowHibernate->GetIsActive());
}

void TorcPower::CanRestartActiveChanged(bool Active)
{
    emit CanRestartChanged(m_allowHibernate->GetValue().toBool() && Active);
}

void TorcPower::CanRestartValueChanged(bool Value)
{
    emit CanRestartChanged(Value && m_allowHibernate->GetIsActive());
}

QVariantMap TorcPower::GetPowerStatus(void)
{
    QVariantMap result;
    result.insert("canShutdown", GetCanShutdown());
    result.insert("canSuspend",  GetCanSuspend());
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
