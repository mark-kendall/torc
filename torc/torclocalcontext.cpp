/* Class TorcLocalContext
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

// Std
#include <signal.h>

// Qt
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QDateTime>
#include <QUuid>
#include <QDir>
#include <QMetaEnum>
#include <QTimer>

// Torc
#include "torccompat.h"
#include "torcevent.h"
#include "torcloggingimp.h"
#include "torclogging.h"
#include "torclanguage.h"
#include "torcexitcodes.h"
#include "torcpower.h"
#include "torclocalcontext.h"
#include "torccoreutils.h"
#include "torcdirectories.h"

TorcLocalContext *gLocalContext = nullptr;

static void ExitHandler(int Sig)
{
    if (SIGPIPE == Sig)
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Received SIGPIPE interrupt - ignoring"));
        return;
    }

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Received %1")
        .arg(Sig == SIGINT ? QStringLiteral("SIGINT") : QStringLiteral("SIGTERM")));

    signal(SIGINT, SIG_DFL);
    TorcLocalContext::NotifyEvent(Torc::Stop);
}

qint16 TorcLocalContext::Create(TorcCommandLine* CommandLine, bool Init /*=true*/)
{
    if (gLocalContext)
        return TORC_EXIT_OK;

    gLocalContext = new TorcLocalContext(CommandLine);
    if (gLocalContext)
        if ((Init && gLocalContext->Init()) || !Init)
            return TORC_EXIT_OK;

    TearDown();
    return TORC_EXIT_NO_CONTEXT;
}

void TorcLocalContext::TearDown(void)
{
    delete gLocalContext;
    gLocalContext = nullptr;
}

void TorcLocalContext::NotifyEvent(int Event)
{
    TorcEvent event(Event);
    if (gLocalContext)
        gLocalContext->Notify(event);
}

/*! \class TorcLocalContext
 *  \brief TorcLocalContext is the core Torc object.
*/
TorcLocalContext::TorcLocalContext(TorcCommandLine* CommandLine)
  : QObject(),
    m_sqliteDB(nullptr),
    m_dbName(QStringLiteral("")),
    m_localSettings(),
    m_localSettingsLock(QReadWriteLock::Recursive),
    m_adminThread(nullptr),
    m_language(nullptr),
    m_uuid(),
    m_shutdownDelay(0),
    m_shutdownEvent(Torc::None),
    m_startTime(QDateTime::currentMSecsSinceEpoch()),
    m_rootSetting(nullptr)
{
    // listen to ourselves:)
    AddObserver(this);

    // set any custom database location
    m_dbName = CommandLine->GetValue(QStringLiteral("db")).toString();

    // Initialise TorcQThread FIRST
    TorcQThread::SetMainThread();

    // install our message handler
    qInstallMessageHandler(&TorcCoreUtils::QtMessage);

    setObjectName(QStringLiteral("LocalContext"));

    // Handle signals gracefully
    signal(SIGINT,  ExitHandler);
    signal(SIGTERM, ExitHandler);
    signal(SIGPIPE, ExitHandler);

    // Initialise local directories
    InitialiseTorcDirectories(CommandLine);

    // Start logging at the first opportunity
    QString logfile = CommandLine->GetValue(QStringLiteral("logfile")).toString();
    if (logfile.isEmpty())
        logfile = QStringLiteral("%1/%2.log").arg(GetTorcConfigDir(), TORC_TORC);

    ParseVerboseArgument(CommandLine->GetValue(QStringLiteral("l")).toString());

    gVerboseMask |= VB_STDIO | VB_FLUSH;

    StartLogging(logfile, 0, 0, CommandLine->GetValue(QStringLiteral("v")).toString(), false);

    // Debug the config directory
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Dir: Using '%1'")
        .arg(GetTorcConfigDir()));

    // Version info
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Version: %1").arg(QStringLiteral(GIT_VERSION)));
    LOG(VB_GENERAL, LOG_NOTICE,
        QStringLiteral("Enabled verbose msgs: %1").arg(gVerboseString));
}

TorcLocalContext::~TorcLocalContext()
{
    // delete language
    delete m_language;

    // close and cleanup the admin thread
    if (m_adminThread)
    {
        m_adminThread->quit();
        m_adminThread->wait();
        delete m_adminThread;
        m_adminThread = nullptr;
    }
    else
    {
        TorcAdminObject::DestroyObjects();
    }

    // wait for threads to exit
    QThreadPool::globalInstance()->waitForDone();

    // remove root setting
    if (m_rootSetting)
    {
        m_rootSetting->Remove();
        m_rootSetting->DownRef();
        m_rootSetting = nullptr;
    }

    // delete the database connection(s)
    delete m_sqliteDB;
    m_sqliteDB = nullptr;

    // revert to the default message handler
    qInstallMessageHandler(nullptr);

    // stop listening to self..
    RemoveObserver(this);

    StopLogging();
}

bool TorcLocalContext::Init(void)
{
    // Create the configuration directory
    QString configdir = GetTorcConfigDir();

    QDir dir(configdir);
    if (!dir.exists())
    {
        if (!dir.mkpath(configdir))
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Failed to create config directory ('%1')")
                .arg(configdir));
            return false;
        }
    }

    // Open the local database
    if (m_dbName.isEmpty())
        m_dbName = configdir + "/" + TORC_TORC + "-settings.sqlite";

    m_sqliteDB = new TorcSQLiteDB(m_dbName);

    if (!m_sqliteDB || !m_sqliteDB->IsValid())
        return false;

    // Load the settings
    {
        QWriteLocker locker(&m_localSettingsLock);
        m_sqliteDB->LoadSettings(m_localSettings);
    }

    // Create the root settings object
    m_rootSetting = new TorcSettingGroup(nullptr, TORC_ROOT_SETTING);

    // create/load the UUID - make this persistent to ensure peers do not think
    // there are multiple devices after a number of restarts.
    QString uuid = QUuid::createUuid().toString();
    if (uuid.startsWith('{'))
        uuid = uuid.mid(1);
    if (uuid.endsWith('}'))
        uuid.chop(1);
    TorcSetting* uuidsaved = new TorcSetting(nullptr, QStringLiteral("uuid"),QStringLiteral("UUID"), TorcSetting::String, TorcSetting::Persistent, QVariant(uuid));
    m_uuid = uuidsaved->GetValue().toString();
    uuidsaved->Remove();
    uuidsaved->DownRef();
    uuidsaved = nullptr;

    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("UUID: %1").arg(m_uuid));

    // Load language and translation preferences
    m_language = new TorcLanguage(m_rootSetting);

    /* We no longer use QRunnables, so ignore this for now at least
    // don't expire threads
    QThreadPool::globalInstance()->setExpiryTimeout(-1);

    // Increase the thread count
    int ideal = QThreadPool::globalInstance()->maxThreadCount();
    int want  = ideal * 8;
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Setting thread pool size to %1 (was %2)").arg(want).arg(ideal));
    QThreadPool::globalInstance()->setMaxThreadCount(want);
    */

    // Bump the UI thread priority
    QThread::currentThread()->setPriority(QThread::TimeCriticalPriority);

    // Qt version?
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Qt runtime version '%1' (compiled with '%2')")
        .arg(qVersion(), QStringLiteral(QT_VERSION_STR)));

    // we don't use an admin thread as purely a server (i.e. no gui) - may need to revisit
#ifdef TORC_TEST
    m_adminThread = new TorcAdminThread();
    m_adminThread->start();
#else
    TorcAdminObject::CreateObjects();
#endif

    return true;
}

QString TorcLocalContext::GetSetting(const QString &Name, const QString &DefaultValue)
{
    return GetDBSetting(Name, DefaultValue);
}

bool TorcLocalContext::GetSetting(const QString &Name, bool DefaultValue)
{
    QString value = GetDBSetting(Name, DefaultValue ? QStringLiteral("1") : QStringLiteral("0"));
    return value.trimmed() == QStringLiteral("1");
}

int TorcLocalContext::GetSetting(const QString &Name, int DefaultValue)
{
    QString value = GetDBSetting(Name, QString::number(DefaultValue));
    return value.toInt();
}

void TorcLocalContext::SetSetting(const QString &Name, const QString &Value)
{
    SetDBSetting(Name, Value);
}

void TorcLocalContext::SetSetting(const QString &Name, bool Value)
{
    SetDBSetting(Name, Value ? QStringLiteral("1") : QStringLiteral("0"));
}

void TorcLocalContext::SetSetting(const QString &Name, int Value)
{
    SetDBSetting(Name, QString::number(Value));
}

QLocale TorcLocalContext::GetLocale(void)
{
    QReadLocker locker(&m_localSettingsLock);
    return m_language->GetLocale();
}

TorcLanguage* TorcLocalContext::GetLanguage(void)
{
    QReadLocker locker(&m_localSettingsLock);
    return m_language;
}

void TorcLocalContext::CloseDatabaseConnections(void)
{
    QReadLocker locker(&m_localSettingsLock);
    if (m_sqliteDB)
        m_sqliteDB->CloseThreadConnection();
}

void TorcLocalContext::SetShutdownDelay(uint Delay)
{
    QWriteLocker locker(&m_localSettingsLock);
    if (Delay < 1 || Delay > 300) // set in XSD
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Not setting shutdown delay to %1: must be 1<->300 seconds").arg(Delay));
    }
    else if (Delay > m_shutdownDelay)
    {
        m_shutdownDelay = Delay;
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Set shutdown delay to %1 seconds").arg(m_shutdownDelay));
    }
}

uint TorcLocalContext::GetShutdownDelay(void)
{
    QReadLocker locker(&m_localSettingsLock);
    return m_shutdownDelay;
}

/*! \brief Register a non-Torc QThread for logging and database access.
 *
 * \note This must be called from the relevant QThread and the QThread must already be named.
*/
void TorcLocalContext::RegisterQThread(void)
{
    RegisterLoggingThread();
}

///\brief Deregister a non-Torc QThread from logging and close any database connections.
void TorcLocalContext::DeregisterQThread(void)
{
    CloseDatabaseConnections();
    DeregisterLoggingThread();
}

void TorcLocalContext::ShutdownTimeout(void)
{
    (void)HandleShutdown(m_shutdownEvent);
}

bool TorcLocalContext::QueueShutdownEvent(int Event)
{
    QWriteLocker locker(&m_localSettingsLock);

    if (m_shutdownDelay < 1)
        return false;

    int newevent = Torc::None;
    switch (Event)
    {
        case Torc::RestartTorc:
        case Torc::Stop:
        case Torc::Shutdown:
        case Torc::Restart:
        case Torc::Hibernate:
        case Torc::Suspend:
            newevent = Event;
            break;
        default: break;
    }

    if (newevent == Torc::None)
        return false;

    if (m_shutdownEvent != Torc::None)
    {
        LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Shutdown already queued - ignoring '%1' event").arg(TorcCoreUtils::EnumToString<Torc::Actions>((Torc::Actions)newevent)));
        return true;
    }

    m_shutdownEvent = newevent;
    LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Queued '%1' event for %2 seconds").arg(TorcCoreUtils::EnumToString<Torc::Actions>((Torc::Actions)m_shutdownEvent)).arg(m_shutdownDelay));
    TorcEvent torcevent(Torc::TorcWillStop);
    Notify(torcevent);
    QTimer::singleShot(m_shutdownDelay * 1000, this, &TorcLocalContext::ShutdownTimeout);
    return true;
}

bool TorcLocalContext::HandleShutdown(int Event)
{
    int event = Event == Torc::None ? m_shutdownEvent : Event;
    switch (event)
    {
        case Torc::RestartTorc:
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Restarting application"));
            TorcReferenceCounter::EventLoopEnding(true);
            QCoreApplication::exit(TORC_EXIT_RESTART);
            return true;
        case Torc::Stop:
            LOG(VB_GENERAL, LOG_INFO, QStringLiteral("Stopping application"));
            TorcReferenceCounter::EventLoopEnding(true);
            QCoreApplication::quit();
            return true;
        case Torc::Suspend:
            {
                TorcEvent event(Torc::SuspendNow);
                Notify(event);
            }
            return true;
        case Torc::Hibernate:
            {
                TorcEvent event(Torc::HibernateNow);
                Notify(event);
            }
            return true;
        case Torc::Shutdown:
            {
                TorcEvent event(Torc::ShutdownNow);
                Notify(event);
            }
            return true;
        case Torc::Restart:
            {
                TorcEvent event(Torc::RestartNow);
                Notify(event);
            }
            return true;
        default:
            break;
    }
    return false;
}

bool TorcLocalContext::event(QEvent *Event)
{
    TorcEvent* torcevent = dynamic_cast<TorcEvent*>(Event);
    if (torcevent)
    {
        int event = torcevent->GetEvent();
        if (QueueShutdownEvent(event))
            return true;
        else if (HandleShutdown(event))
            return true;
    }

    return QObject::event(Event);
}

QString TorcLocalContext::GetUuid(void) const
{
    return m_uuid;
}

TorcSetting* TorcLocalContext::GetRootSetting(void)
{
    QReadLocker locker(&m_localSettingsLock);
    return m_rootSetting;
}

qint64 TorcLocalContext::GetStartTime(void)
{
    QReadLocker locker(&m_localSettingsLock);
    return m_startTime;
}

int TorcLocalContext::GetPriority(void)
{
    return 0;
}

QString TorcLocalContext::GetDBSetting(const QString &Name, const QString &DefaultValue)
{
    {
        QReadLocker locker(&m_localSettingsLock);
        if (m_localSettings.contains(Name))
            return m_localSettings.value(Name);
    }

    SetDBSetting(Name, DefaultValue);
    return DefaultValue;
}

void TorcLocalContext::SetDBSetting(const QString &Name, const QString &Value)
{
    QWriteLocker locker(&m_localSettingsLock);
    if (m_sqliteDB)
        m_sqliteDB->SetSetting(Name, Value);
    m_localSettings[Name] = Value;
}
