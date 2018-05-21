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
   TorcHTTPService(this, "user", "user", TorcUser::staticMetaObject, ""),
   m_userName(),
   m_userNameSetting(new TorcSetting(NULL, "UserName", "User name", TorcSetting::String, TorcSetting::Persistent, QVariant(TORC_DEFAULT_USERNAME))),
   m_userCredentials(new TorcSetting(NULL, "UserCredentials", "Authentication string", TorcSetting::String, TorcSetting::Persistent, QVariant(TORC_DEFAULT_CREDENTIALS))),
   m_canRestartTorc(true),
   m_canStopTorc(true),
   m_lock(QMutex::Recursive)
{
    {
        QMutexLocker locker(&gUserCredentialsLock);
        gUserName = m_userNameSetting->GetValue().toString();
        gUserCredentials = m_userCredentials->GetValue().toString().toLower().toLatin1();
        m_userName = gUserName;
    }

    connect(m_userNameSetting, SIGNAL(ValueChanged(QString)), this, SLOT(UpdateUserName(QString)));
    connect(m_userCredentials, SIGNAL(ValueChanged(QString)), this, SLOT(UpdateCredentials(QString)));
}

TorcUser::~TorcUser()
{
    m_userNameSetting->Remove();
    m_userNameSetting->DownRef();
    m_userNameSetting = NULL;
    m_userCredentials->Remove();
    m_userCredentials->DownRef();
    m_userCredentials = NULL;
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

void TorcUser::UpdateUserName(const QString &Name)
{
    QMutexLocker locker(&gUserCredentialsLock);
    m_userName = Name;
    gUserName  = Name;
}

void TorcUser::UpdateCredentials(const QString &Credentials)
{
    QMutexLocker locker(&gUserCredentialsLock);
    gUserCredentials = Credentials.toLower().toLatin1();
}

bool TorcUser::SetUserCredentials(const QString &Name, const QString &Credentials)
{
    QMutexLocker locker(&m_lock);

    if (Name == gUserName && Credentials.toLower() == GetCredentials())
        return false;

    if (Name.isEmpty())
        return false;

    // N.B. MD5 credentials are a hexadecimal representation of a binary. As such they can
    // contain either upper or lower case digits.
    static QRegExp gReg("[0-9a-fA-F]{32}");
    if (!gReg.exactMatch(Credentials))
        return false;

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
    QMutexLocker locker(&m_lock);
    return m_canStopTorc;
}

void TorcUser::RestartTorc(void)
{
    // NB could be called from any thread
    TorcLocalContext::NotifyEvent(Torc::RestartTorc);
}

bool TorcUser::GetCanRestartTorc(void)
{
    QMutexLocker locker(&m_lock);
    return m_canRestartTorc;
}

class TorcUserObject : public TorcAdminObject, public TorcStringFactory
{
    Q_DECLARE_TR_FUNCTIONS(TorcUserObject)

  public:
    TorcUserObject() : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY)
    {
    }

    ~TorcUserObject()
    {
    }

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
    }

    void Create(void)
    {
    }

    void Destroy(void)
    {
    }
} TorcUserObject;
