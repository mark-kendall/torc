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

#define TORC_DEFAULT_USERNAME    QString("admin")
#define TORC_DEFAULT_CREDENTIALS QString("f0f825afb7ee5ca70ba178463f360d4b")

QString    TorcUser::gUserName = QString();
QByteArray TorcUser::gUserCredentials = QByteArray();
QMutex     TorcUser::gUserCredentialsLock(QMutex::Recursive);

TorcUser::TorcUser()
 : QObject(),
   TorcHTTPService(this, TORC_USER_SERVICE, "user", TorcUser::staticMetaObject, ""),
   m_userName(),
   m_userNameSetting(new TorcSetting(nullptr, "UserName", "User name", TorcSetting::String, TorcSetting::Persistent, QVariant(TORC_DEFAULT_USERNAME))),
   m_userCredentials(new TorcSetting(nullptr, "UserCredentials", "Authentication string", TorcSetting::String, TorcSetting::Persistent, QVariant(TORC_DEFAULT_CREDENTIALS))),
   m_canRestartTorc(true),
   m_canStopTorc(true)
{
    {
        QMutexLocker locker(&gUserCredentialsLock);
        gUserName = m_userNameSetting->GetValue().toString();
        gUserCredentials = m_userCredentials->GetValue().toString().toLower().toLatin1();
        m_userName = gUserName;
    }

    connect(m_userNameSetting, QOverload<QString&>::of(&TorcSetting::ValueChanged), this, &TorcUser::UpdateUserName);
    connect(m_userCredentials, QOverload<QString&>::of(&TorcSetting::ValueChanged), this, &TorcUser::UpdateCredentials);
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
        LOG(VB_GENERAL, LOG_WARNING, "New credentials match old - not changing");
        return false;
    }

    // enforce minimum size (4) and alphanumeric and underscore
    static QRegExp gReg1("[\\w]{4,}");
    if (!gReg1.exactMatch(Name))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Password unacceptable");
        return false;
    }

    // N.B. MD5 credentials are a hexadecimal representation of a binary. As such they can
    // contain either upper or lower case digits.
    static QRegExp gReg2("[0-9a-fA-F]{32}");
    if (!gReg2.exactMatch(Credentials))
    {
        LOG(VB_GENERAL, LOG_WARNING, "User credentials hash unacceptable");
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
        Strings.insert("LoggedInUserTr",     QCoreApplication::translate("TorcUser", "Logged in as %1"));
        Strings.insert("RestartTorcTr",      QCoreApplication::translate("TorcUser", "Restart Torc"));
        Strings.insert("ConfirmRestartTorc", QCoreApplication::translate("TorcUser", "Are you sure you want to restart Torc?"));
        Strings.insert("StopTorcTr",         QCoreApplication::translate("TorcUser", "Stop Torc"));
        Strings.insert("ConfirmStopTorc",    QCoreApplication::translate("TorcUser", "Are you sure you want to stop Torc?"));
        Strings.insert("ViewConfigTr",       QCoreApplication::translate("TorcUser", "View configuration"));
        Strings.insert("ViewConfigTitleTr",  QCoreApplication::translate("TorcUser", "Current configuration"));
        Strings.insert("ViewDOTTr",          QCoreApplication::translate("TorcUser", "View DOT"));
        Strings.insert("ViewDOTTitleTr",     QCoreApplication::translate("TorcUser", "Stategraph Description"));
        Strings.insert("ViewXSDTr",          QCoreApplication::translate("TorcUser", "View XSD"));
        Strings.insert("ViewXSDTitleTr",     QCoreApplication::translate("TorcUser", "Configuration schema"));
        Strings.insert("ViewAPITr",          QCoreApplication::translate("TorcUser", "View API"));
        Strings.insert("ViewAPITitleTr",     QCoreApplication::translate("TorcUser", "API reference"));
        Strings.insert("ViewLogTr",          QCoreApplication::translate("TorcUser", "View Log"));
        Strings.insert("RefreshTr",          QCoreApplication::translate("TorcUser", "Refresh"));
        Strings.insert("FollowLogTr",        QCoreApplication::translate("TorcUser", "Follow Log"));
        Strings.insert("FollowTr",           QCoreApplication::translate("TorcUser", "Follow"));
        Strings.insert("UnfollowTr",         QCoreApplication::translate("TorcUser", "Unfollow"));
        Strings.insert("Value",              QCoreApplication::translate("TorcUser", "Value"));
        Strings.insert("Valid",              QCoreApplication::translate("TorcUser", "Valid"));
        Strings.insert("CloseTr",            QCoreApplication::translate("TorcUser", "Close"));
        Strings.insert("SettingsTr",         QCoreApplication::translate("TorcUser", "Settings"));
        Strings.insert("ConfirmTr",          QCoreApplication::translate("TorcUser", "Confirm"));
        Strings.insert("CancelTr",           QCoreApplication::translate("TorcUser", "Cancel"));
        Strings.insert("ChangeCredsTr",      QCoreApplication::translate("TorcUser", "Change username and password"));
        Strings.insert("UsernameTr",         QCoreApplication::translate("TorcUser", "Username"));
        Strings.insert("PasswordTr",         QCoreApplication::translate("TorcUser", "Password"));
        Strings.insert("Username2Tr",        QCoreApplication::translate("TorcUser", "Confirm username"));
        Strings.insert("Password2Tr",        QCoreApplication::translate("TorcUser", "Confirm password"));
        Strings.insert("CredentialsHelpTr",  QCoreApplication::translate("TorcUser", "Usernames and passwords are case sensitive, must be at least 4 characters long and can only contain letters, numbers and \'_\' (underscore)."));
        Strings.insert("ServicesTr",         QCoreApplication::translate("TorcUser", "Services"));
        Strings.insert("ReturnformatsTr",    QCoreApplication::translate("TorcUser", "Return formats"));
        Strings.insert("WebSocketsTr",       QCoreApplication::translate("TorcUser", "WebSockets"));
        Strings.insert("AvailableservicesTr",QCoreApplication::translate("TorcUser", "Available services"));
        Strings.insert("IDTr",               QCoreApplication::translate("TorcUser", "ID"));
        Strings.insert("NameTr",             QCoreApplication::translate("TorcUser", "Name"));
        Strings.insert("PathTr",             QCoreApplication::translate("TorcUser", "Path"));
        Strings.insert("DetailsTr",          QCoreApplication::translate("TorcUser", "Details"));
        Strings.insert("HTTPreturnformatsTr",QCoreApplication::translate("TorcUser", "Supported HTTP return formats"));
        Strings.insert("ContenttypeTr",      QCoreApplication::translate("TorcUser", "Content type"));
        Strings.insert("WSsubprotocolsTr",   QCoreApplication::translate("TorcUser", "Supported WebSocket subprotocols"));
        Strings.insert("DescriptionTr",      QCoreApplication::translate("TorcUser", "Description"));
        Strings.insert("ParametersTr",       QCoreApplication::translate("TorcUser", "Parameters"));
        Strings.insert("JSreturntypeTr",     QCoreApplication::translate("TorcUser", "Javascript return type"));
        Strings.insert("MethodlistTr",       QCoreApplication::translate("TorcUser", "Method list"));
        Strings.insert("GetterTr",           QCoreApplication::translate("TorcUser", "Getter"));
        Strings.insert("NotificationTr",     QCoreApplication::translate("TorcUser", "Notification"));
        Strings.insert("UpdateTr",           QCoreApplication::translate("TorcUser", "Update"));
    }

    void Create(void)
    {
    }

    void Destroy(void)
    {
    }
} TorcUserObject;
