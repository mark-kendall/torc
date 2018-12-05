/* Class TorcUser
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2018
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
#include <QRegExp>
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torclanguage.h"
#include "torcuser.h"

#define TORC_DEFAULT_USERNAME    QStringLiteral("admin")
#define TORC_DEFAULT_CREDENTIALS QStringLiteral("f0f825afb7ee5ca70ba178463f360d4b")

QString    TorcUser::gUserName = QStringLiteral();
QByteArray TorcUser::gUserCredentials = QByteArray();
QMutex     TorcUser::gUserCredentialsLock(QMutex::Recursive);

TorcUser::TorcUser()
 : QObject(),
   TorcHTTPService(this, TORC_USER_SERVICE, QStringLiteral("user"), TorcUser::staticMetaObject, QStringLiteral("")),
   m_userName(),
   m_userNameSetting(new TorcSetting(nullptr, QStringLiteral("UserName"), QStringLiteral("User name"), TorcSetting::String, TorcSetting::Persistent, QVariant(TORC_DEFAULT_USERNAME))),
   m_userCredentials(new TorcSetting(nullptr, QStringLiteral("UserCredentials"), QStringLiteral("Authentication string"), TorcSetting::String, TorcSetting::Persistent, QVariant(TORC_DEFAULT_CREDENTIALS))),
   m_canRestartTorc(true),
   m_canStopTorc(true)
{
    {
        QMutexLocker locker(&gUserCredentialsLock);
        gUserName = m_userNameSetting->GetValue().toString();
        gUserCredentials = m_userCredentials->GetValue().toString().toLower().toLatin1();
        m_userName = gUserName;
    }

    connect(m_userNameSetting, static_cast<void (TorcSetting::*)(QString&)>(&TorcSetting::ValueChanged), this, &TorcUser::UpdateUserName);
    connect(m_userCredentials, static_cast<void (TorcSetting::*)(QString&)>(&TorcSetting::ValueChanged), this, &TorcUser::UpdateCredentials);
}

TorcUser::~TorcUser()
{
    m_userNameSetting->Remove();
    m_userNameSetting->DownRef();
    m_userCredentials->Remove();
    m_userCredentials->DownRef();
}

QString TorcUser::GetName(void)
{
    QMutexLocker locker(&gUserCredentialsLock);
    return gUserName;
}

QString TorcUser::GetUserName(void)
{
    QMutexLocker locker(&gUserCredentialsLock);
    return m_userName;
}

QByteArray TorcUser::GetCredentials(void)
{
    QMutexLocker locker(&gUserCredentialsLock);
    return gUserCredentials;
}

void TorcUser::UpdateUserName(QString &Name)
{
    QMutexLocker locker(&gUserCredentialsLock);
    m_userName = Name;
    gUserName  = Name;
}

void TorcUser::UpdateCredentials(QString &Credentials)
{
    QMutexLocker locker(&gUserCredentialsLock);
    gUserCredentials = Credentials.toLower().toLatin1();
}

bool TorcUser::SetUserCredentials(const QString &Name, const QString &Credentials)
{
    QWriteLocker locker(&m_httpServiceLock);

    if (Name == gUserName && Credentials.toLower() == GetCredentials())
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("New credentials match old - not changing"));
        return false;
    }

    // enforce minimum size (4) and alphanumeric and underscore
    static QRegExp gReg1("[\\w]{4,}");
    if (!gReg1.exactMatch(Name))
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("Password unacceptable"));
        return false;
    }

    // N.B. MD5 credentials are a hexadecimal representation of a binary. As such they can
    // contain either upper or lower case digits.
    static QRegExp gReg2("[0-9a-fA-F]{32}");
    if (!gReg2.exactMatch(Credentials))
    {
        LOG(VB_GENERAL, LOG_WARNING, QStringLiteral("User credentials hash unacceptable"));
        return false;
    }

    // but an MD5 hash with upper case letters produces different results when
    // rehashed... so set to lower
    m_userNameSetting->SetValue(QString(Name));
    m_userCredentials->SetValue(QString(Credentials.toLower()));

    TorcLocalContext::NotifyEvent(Torc::UserChanged);
    return true;
}

void TorcUser::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

void TorcUser::StopTorc(void)
{
    TorcLocalContext::NotifyEvent(Torc::Stop);
}

bool TorcUser::GetCanStopTorc(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return m_canStopTorc;
}

void TorcUser::RestartTorc(void)
{
    // NB could be called from any thread
    TorcLocalContext::NotifyEvent(Torc::RestartTorc);
}

bool TorcUser::GetCanRestartTorc(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return m_canRestartTorc;
}

class TorcUserObject : public TorcAdminObject, public TorcStringFactory
{
    Q_DECLARE_TR_FUNCTIONS(TorcUserObject)

  public:
    TorcUserObject() : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY)
    {
    }

    ~TorcUserObject() = default;

    void GetStrings(QVariantMap &Strings)
    {
        Strings.insert(QStringLiteral("LoggedInUserTr"),     QCoreApplication::translate("TorcUser", "Logged in as %1"));
        Strings.insert(QStringLiteral("RestartTorcTr"),      QCoreApplication::translate("TorcUser", "Restart Torc"));
        Strings.insert(QStringLiteral("ConfirmRestartTorc"), QCoreApplication::translate("TorcUser", "Are you sure you want to restart Torc?"));
        Strings.insert(QStringLiteral("StopTorcTr"),         QCoreApplication::translate("TorcUser", "Stop Torc"));
        Strings.insert(QStringLiteral("ConfirmStopTorc"),    QCoreApplication::translate("TorcUser", "Are you sure you want to stop Torc?"));
        Strings.insert(QStringLiteral("ViewConfigTr"),       QCoreApplication::translate("TorcUser", "View configuration"));
        Strings.insert(QStringLiteral("ViewConfigTitleTr"),  QCoreApplication::translate("TorcUser", "Current configuration"));
        Strings.insert(QStringLiteral("ViewDOTTr"),          QCoreApplication::translate("TorcUser", "View DOT"));
        Strings.insert(QStringLiteral("ViewDOTTitleTr"),     QCoreApplication::translate("TorcUser", "Stategraph Description"));
        Strings.insert(QStringLiteral("ViewXSDTr"),          QCoreApplication::translate("TorcUser", "View XSD"));
        Strings.insert(QStringLiteral("ViewXSDTitleTr"),     QCoreApplication::translate("TorcUser", "Configuration schema"));
        Strings.insert(QStringLiteral("ViewAPITr"),          QCoreApplication::translate("TorcUser", "View API"));
        Strings.insert(QStringLiteral("ViewAPITitleTr"),     QCoreApplication::translate("TorcUser", "API reference"));
        Strings.insert(QStringLiteral("ViewLogTr"),          QCoreApplication::translate("TorcUser", "View Log"));
        Strings.insert(QStringLiteral("RefreshTr"),          QCoreApplication::translate("TorcUser", "Refresh"));
        Strings.insert(QStringLiteral("FollowLogTr"),        QCoreApplication::translate("TorcUser", "Follow Log"));
        Strings.insert(QStringLiteral("FollowTr"),           QCoreApplication::translate("TorcUser", "Follow"));
        Strings.insert(QStringLiteral("UnfollowTr"),         QCoreApplication::translate("TorcUser", "Unfollow"));
        Strings.insert(QStringLiteral("Value"),              QCoreApplication::translate("TorcUser", "Value"));
        Strings.insert(QStringLiteral("Valid"),              QCoreApplication::translate("TorcUser", "Valid"));
        Strings.insert(QStringLiteral("CloseTr"),            QCoreApplication::translate("TorcUser", "Close"));
        Strings.insert(QStringLiteral("SettingsTr"),         QCoreApplication::translate("TorcUser", "Settings"));
        Strings.insert(QStringLiteral("ConfirmTr"),          QCoreApplication::translate("TorcUser", "Confirm"));
        Strings.insert(QStringLiteral("CancelTr"),           QCoreApplication::translate("TorcUser", "Cancel"));
        Strings.insert(QStringLiteral("ChangeCredsTr"),      QCoreApplication::translate("TorcUser", "Change username and password"));
        Strings.insert(QStringLiteral("UsernameTr"),         QCoreApplication::translate("TorcUser", "Username"));
        Strings.insert(QStringLiteral("PasswordTr"),         QCoreApplication::translate("TorcUser", "Password"));
        Strings.insert(QStringLiteral("Username2Tr"),        QCoreApplication::translate("TorcUser", "Confirm username"));
        Strings.insert(QStringLiteral("Password2Tr"),        QCoreApplication::translate("TorcUser", "Confirm password"));
        Strings.insert(QStringLiteral("CredentialsHelpTr"),  QCoreApplication::translate("TorcUser", "Usernames and passwords are case sensitive, must be at least 4 characters long and can only contain letters, numbers and \'_\' (underscore)."));
        Strings.insert(QStringLiteral("ServicesTr"),         QCoreApplication::translate("TorcUser", "Services"));
        Strings.insert(QStringLiteral("ReturnformatsTr"),    QCoreApplication::translate("TorcUser", "Return formats"));
        Strings.insert(QStringLiteral("WebSocketsTr"),       QCoreApplication::translate("TorcUser", "WebSockets"));
        Strings.insert(QStringLiteral("AvailableservicesTr"),QCoreApplication::translate("TorcUser", "Available services"));
        Strings.insert(QStringLiteral("IDTr"),               QCoreApplication::translate("TorcUser", "ID"));
        Strings.insert(QStringLiteral("NameTr"),             QCoreApplication::translate("TorcUser", "Name"));
        Strings.insert(QStringLiteral("PathTr"),             QCoreApplication::translate("TorcUser", "Path"));
        Strings.insert(QStringLiteral("DetailsTr"),          QCoreApplication::translate("TorcUser", "Details"));
        Strings.insert(QStringLiteral("HTTPreturnformatsTr"),QCoreApplication::translate("TorcUser", "Supported HTTP return formats"));
        Strings.insert(QStringLiteral("ContenttypeTr"),      QCoreApplication::translate("TorcUser", "Content type"));
        Strings.insert(QStringLiteral("WSsubprotocolsTr"),   QCoreApplication::translate("TorcUser", "Supported WebSocket subprotocols"));
        Strings.insert(QStringLiteral("DescriptionTr"),      QCoreApplication::translate("TorcUser", "Description"));
        Strings.insert(QStringLiteral("ParametersTr"),       QCoreApplication::translate("TorcUser", "Parameters"));
        Strings.insert(QStringLiteral("JSreturntypeTr"),     QCoreApplication::translate("TorcUser", "Javascript return type"));
        Strings.insert(QStringLiteral("MethodlistTr"),       QCoreApplication::translate("TorcUser", "Method list"));
        Strings.insert(QStringLiteral("GetterTr"),           QCoreApplication::translate("TorcUser", "Getter"));
        Strings.insert(QStringLiteral("NotificationTr"),     QCoreApplication::translate("TorcUser", "Notification"));
        Strings.insert(QStringLiteral("UpdateTr"),           QCoreApplication::translate("TorcUser", "Update"));
    }

    void Create(void)
    {
    }

    void Destroy(void)
    {
    }
} TorcUserObject;
