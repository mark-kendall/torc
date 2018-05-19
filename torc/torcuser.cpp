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
#include <QMutex>

// Torc
#include "torclocalcontext.h"
#include "torcadminthread.h"
#include "torclanguage.h"
#include "torcuser.h"

TorcUser::TorcUser()
 : QObject(),
   TorcHTTPService(this, "user", "user", TorcUser::staticMetaObject, ""),
   canRestartTorc(true),
   canStopTorc(true),
   m_lock(QMutex::Recursive)
{
}

TorcUser::~TorcUser()
{
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
    return canStopTorc;
}

void TorcUser::RestartTorc(void)
{
    // NB could be called from any thread
    TorcLocalContext::NotifyEvent(Torc::RestartTorc);
}

bool TorcUser::GetCanRestartTorc(void)
{
    QMutexLocker locker(&m_lock);
    return canRestartTorc;
}

class TorcUserObject : public TorcAdminObject, public TorcStringFactory
{
    Q_DECLARE_TR_FUNCTIONS(TorcUserObject)

  public:
    TorcUserObject()
      : TorcAdminObject(TORC_ADMIN_LOW_PRIORITY),
        m_object(NULL)
    {
    }

    ~TorcUserObject()
    {
    }

    void GetStrings(QVariantMap &Strings)
    {
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
    }

    void Create(void)
    {
        Destroy();
        m_object = new TorcUser();
    }

    void Destroy(void)
    {
        delete m_object;
        m_object = NULL;
    }

  private:
    Q_DISABLE_COPY(TorcUserObject)
    TorcUser *m_object;
} TorcUserObject;
