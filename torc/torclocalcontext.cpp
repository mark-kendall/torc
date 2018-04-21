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
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QThreadPool>
#include <QDateTime>
#include <QMutex>
#include <QUuid>
#include <QDir>
#include <QMetaEnum>

// Torc
#include "torcevent.h"
#include "torcloggingimp.h"
#include "torclogging.h"
#include "torclanguage.h"
#include "torcexitcodes.h"
#include "torcsqlitedb.h"
#include "torcadminthread.h"
#include "torcpower.h"
#include "torclocalcontext.h"
#include "torccoreutils.h"
#include "torcdirectories.h"

TorcLocalContext *gLocalContext = NULL;
QMutex*            gAVCodecLock = new QMutex(QMutex::Recursive);
TorcSetting       *gRootSetting = NULL;
qint64             gStartTime   = QDateTime::currentMSecsSinceEpoch();

static void ExitHandler(int Sig)
{
    signal(SIGINT, SIG_DFL);
    LOG(VB_GENERAL, LOG_INFO, QString("Received %1")
        .arg(Sig == SIGINT ? "SIGINT" : "SIGTERM"));

    TorcLocalContext::NotifyEvent(Torc::Stop);
}

class TorcLocalContextPriv
{
  public:
    explicit TorcLocalContextPriv(TorcCommandLine *CommandLine);
   ~TorcLocalContextPriv();

    bool    Init                 (void);
    QString GetSetting           (const QString &Name, const QString &DefaultValue);
    void    SetSetting           (const QString &Name, const QString &Value);
    QString GetPreference        (const QString &Name, const QString &DefaultValue);
    void    SetPreference        (const QString &Name, const QString &Value);
    QString GetUuid              (void);

    TorcSQLiteDB         *m_sqliteDB;
    QString               m_dbName;
    QMap<QString,QString> m_localSettings;
    QReadWriteLock       *m_localSettingsLock;
    QMap<QString,QString> m_preferences;
    QReadWriteLock       *m_preferencesLock;
    QObject              *m_UIObject;
    TorcAdminThread      *m_adminThread;
    TorcLanguage         *m_language;
    QString               m_uuid;
};

TorcLocalContextPriv::TorcLocalContextPriv(TorcCommandLine *CommandLine)
  : m_sqliteDB(NULL),
    m_dbName(QString("")),
    m_localSettingsLock(new QReadWriteLock(QReadWriteLock::Recursive)),
    m_preferencesLock(new QReadWriteLock(QReadWriteLock::Recursive)),
    m_UIObject(NULL),
    m_adminThread(NULL),
    m_language(NULL)
{
    // set any custom database location
    m_dbName = CommandLine->GetValue("db").toString();
}

TorcLocalContextPriv::~TorcLocalContextPriv()
{
    // delete language
    delete m_language;

    // close and cleanup the admin thread
    if (m_adminThread)
    {
        m_adminThread->quit();
        m_adminThread->wait();
        delete m_adminThread;
        m_adminThread = NULL;
    }
    else
    {
        TorcAdminObject::DestroyObjects();
    }

    // wait for threads to exit
    QThreadPool::globalInstance()->waitForDone();

    // remove root setting
    if (gRootSetting)
    {
        gRootSetting->Remove();
        gRootSetting->DownRef();
        gRootSetting = NULL;
    }

    // delete the database connection(s)
    delete m_sqliteDB;
    m_sqliteDB = NULL;

    // delete settings lock
    delete m_localSettingsLock;
    m_localSettingsLock = NULL;

    // delete preferences lock
    delete m_preferencesLock;
    m_preferencesLock = NULL;
}

bool TorcLocalContextPriv::Init(void)
{
    // Create the configuration directory
    QString configdir = GetTorcConfigDir();

    QDir dir(configdir);
    if (!dir.exists())
    {
        if (!dir.mkdir(configdir))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to create config directory ('%1')")
                .arg(configdir));
            return false;
        }
    }

    // Load language and translation preferences
    m_language = new TorcLanguage();
    m_language->LoadPreferences();

    // Open the local database
    if (m_dbName.isEmpty())
        m_dbName = configdir + "/" + QCoreApplication::applicationName() + "-settings.sqlite";

    m_sqliteDB = new TorcSQLiteDB(m_dbName);

    if (!m_sqliteDB || !m_sqliteDB->IsValid())
        return false;

    // Load the settings
    {
        QWriteLocker locker(m_localSettingsLock);
        m_sqliteDB->LoadSettings(m_localSettings);
    }

    // Load preferences
    {
        QWriteLocker locker(m_preferencesLock);
        m_sqliteDB->LoadPreferences(m_preferences);
    }

    // Create the root settings object
    gRootSetting = new TorcSettingGroup(NULL, QString("RootSetting"));

    // create/load the UUID - make this persistent to ensure peers do not think
    // there are multiple devices after a number of restarts.
    QString uuid = QUuid::createUuid().toString();
    if (uuid.startsWith('{'))
        uuid = uuid.mid(1);
    if (uuid.endsWith('}'))
        uuid.chop(1);
    TorcSetting* uuidsaved = new TorcSetting(NULL, QString("uuid"),QString("UUID"), TorcSetting::Checkbox, true, QVariant(uuid));
    m_uuid = uuidsaved->GetValue().toString();
    uuidsaved->Remove();
    uuidsaved->DownRef();
    uuidsaved = NULL;

    LOG(VB_GENERAL, LOG_INFO, QString("UUID: %1").arg(m_uuid));

    // don't expire threads
    QThreadPool::globalInstance()->setExpiryTimeout(-1);

    // Increase the thread count
    int ideal = QThreadPool::globalInstance()->maxThreadCount();
    int want  = ideal * 8;
    LOG(VB_GENERAL, LOG_INFO, QString("Setting thread pool size to %1 (was %2)").arg(want).arg(ideal));
    QThreadPool::globalInstance()->setMaxThreadCount(want);

    // Bump the UI thread priority
    QThread::currentThread()->setPriority(QThread::TimeCriticalPriority);

    // Qt version?
    LOG(VB_GENERAL, LOG_INFO, QString("Qt runtime version '%1' (compiled with '%2')")
        .arg(qVersion()).arg(QT_VERSION_STR));

    // we don't use an admin thread as purely a server (i.e. no gui) - may need to revisit
    TorcAdminObject::CreateObjects();

    return true;
}

QString TorcLocalContextPriv::GetSetting(const QString &Name,
                                         const QString &DefaultValue)
{
    {
        QReadLocker locker(m_localSettingsLock);
        if (m_localSettings.contains(Name))
            return m_localSettings.value(Name);
    }

    SetSetting(Name, DefaultValue);
    return DefaultValue;
}

void TorcLocalContextPriv::SetSetting(const QString &Name, const QString &Value)
{
    QWriteLocker locker(m_localSettingsLock);
    if (m_sqliteDB)
        m_sqliteDB->SetSetting(Name, Value);
    m_localSettings[Name] = Value;
}

QString TorcLocalContextPriv::GetPreference(const QString &Name, const QString &DefaultValue)
{
    {
        QReadLocker locker(m_preferencesLock);

        if (m_preferences.contains(Name))
            return m_preferences.value(Name);

        if (m_preferences.contains(Name))
            return m_preferences.value(Name);
    }

    SetPreference(Name, DefaultValue);
    return DefaultValue;
}

void TorcLocalContextPriv::SetPreference(const QString &Name, const QString &Value)
{
    QWriteLocker locker(m_preferencesLock);
    if (m_sqliteDB)
        m_sqliteDB->SetPreference(Name, Value);
    m_preferences[Name] = Value;
}

QString TorcLocalContextPriv::GetUuid(void)
{
    return m_uuid;
}

QString Torc::ActionToString(Actions Action)
{
    const QMetaObject &mo = Torc::staticMetaObject;
    int enum_index        = mo.indexOfEnumerator("Actions");
    QMetaEnum metaEnum    = mo.enumerator(enum_index);
    return metaEnum.valueToKey(Action);
}

int Torc::StringToAction(const QString &Action)
{
    const QMetaObject &mo = Torc::staticMetaObject;
    int enum_index        = mo.indexOfEnumerator("Actions");
    QMetaEnum metaEnum    = mo.enumerator(enum_index);
    return metaEnum.keyToValue(Action.toLatin1());
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
    gLocalContext = NULL;
}

void TorcLocalContext::NotifyEvent(int Event)
{
    TorcEvent event(Event);
    if (gLocalContext)
        gLocalContext->Notify(event);
}

/*! \class TorcLocalContext
 *  \brief TorcLocalContext is the core Torc object.
 *
 * \todo Add priority generation based on role and maybe user setting.
 * \todo Convert Q_INVOKABLEs to public slots.
*/
TorcLocalContext::TorcLocalContext(TorcCommandLine* CommandLine)
  : QObject(),
    m_priv(new TorcLocalContextPriv(CommandLine))
{
    // Initialise TorcQThread FIRST
    TorcQThread::SetMainThread();

    // install our message handler
    qInstallMessageHandler(&TorcCoreUtils::QtMessage);

    setObjectName("LocalContext");

    // Handle signals gracefully
    signal(SIGINT,  ExitHandler);
    signal(SIGTERM, ExitHandler);

    // Initialise local directories
    InitialiseTorcDirectories();

    // Start logging at the first opportunity
    QString logfile = CommandLine->GetValue("logfile").toString();
    if (logfile.isEmpty())
        logfile = QString(GetTorcConfigDir() + "/" + QCoreApplication::applicationName() + ".log");

    ParseVerboseArgument(CommandLine->GetValue("l").toString());

    gVerboseMask |= VB_STDIO | VB_FLUSH;

    StartLogging(logfile, 0, 0, CommandLine->GetValue("v").toString(), false);

    // Debug the config directory
    LOG(VB_GENERAL, LOG_INFO, QString("Dir: Using '%1'")
        .arg(GetTorcConfigDir()));

    // Version info
    LOG(VB_GENERAL, LOG_INFO, QString("Version: %1").arg(GIT_VERSION));
    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Enabled verbose msgs: %1").arg(gVerboseString));
}

TorcLocalContext::~TorcLocalContext()
{
    delete m_priv;
    m_priv = NULL;

    StopLogging();

    // revert to the default message handler
    qInstallMessageHandler(0);
}

bool TorcLocalContext::Init(void)
{
    if (!m_priv->Init())
        return false;

    return true;
}

QString TorcLocalContext::GetSetting(const QString &Name, const QString &DefaultValue)
{
    return m_priv->GetSetting(Name, DefaultValue);
}

bool TorcLocalContext::GetSetting(const QString &Name, const bool &DefaultValue)
{
    QString value = GetSetting(Name, DefaultValue ? QString("1") : QString("0"));
    return value.trimmed() == "1";
}

int TorcLocalContext::GetSetting(const QString &Name, const int &DefaultValue)
{
    QString value = GetSetting(Name, QString::number(DefaultValue));
    return value.toInt();
}

void TorcLocalContext::SetSetting(const QString &Name, const QString &Value)
{
    m_priv->SetSetting(Name, Value);
}

void TorcLocalContext::SetSetting(const QString &Name, const bool &Value)
{
    m_priv->SetSetting(Name, Value ? QString("1") : QString("0"));
}

void TorcLocalContext::SetSetting(const QString &Name, const int &Value)
{
    m_priv->SetSetting(Name, QString::number(Value));
}

QString TorcLocalContext::GetPreference(const QString &Name, const QString &DefaultValue)
{
    return m_priv->GetPreference(Name, DefaultValue);
}

bool TorcLocalContext::GetPreference(const QString &Name, const bool &DefaultValue)
{
    QString value = GetPreference(Name, DefaultValue ? QString("1") : QString("0"));
    return value.trimmed() == "1";
}

int TorcLocalContext::GetPreference(const QString &Name, const int &DefaultValue)
{
    QString value = GetPreference(Name, QString::number(DefaultValue));
    return value.toInt();
}

void TorcLocalContext::SetPreference(const QString &Name, const QString &Value)
{
    m_priv->SetPreference(Name, Value);
}

void TorcLocalContext::SetPreference(const QString &Name, const bool &Value)
{
    m_priv->SetPreference(Name, Value ? QString("1") : QString("0"));
}

void TorcLocalContext::SetPreference(const QString &Name, const int &Value)
{
    m_priv->SetPreference(Name, QString::number(Value));
}

void TorcLocalContext::SetUIObject(QObject *UI)
{
    if (m_priv->m_UIObject && UI)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Trying to register a second global UI - ignoring");
        return;
    }

    m_priv->m_UIObject = UI;
}

QObject* TorcLocalContext::GetUIObject(void)
{
    return m_priv->m_UIObject;
}

QLocale TorcLocalContext::GetLocale(void)
{
    return m_priv->m_language->GetLocale();
}

TorcLanguage* TorcLocalContext::GetLanguage(void)
{
    return m_priv->m_language;
}

void TorcLocalContext::CloseDatabaseConnections(void)
{
    if (m_priv && m_priv->m_sqliteDB)
        m_priv->m_sqliteDB->CloseThreadConnection();
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

QString TorcLocalContext::GetUuid(void)
{
    return m_priv->GetUuid();
}

TorcSetting* TorcLocalContext::GetRootSetting(void)
{
    return gRootSetting;
}

qint64 TorcLocalContext::GetStartTime(void)
{
    return gStartTime;
}

int TorcLocalContext::GetPriority(void)
{
    return 0;
}
