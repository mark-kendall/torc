/* Class TorcSetting
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2013-18
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

// Torc
#include "torclocalcontext.h"
#include "torccoreutils.h"
#include "torcsetting.h"

/*! \class TorcSetting
 *  \brief A wrapper around a database setting.
 *
 * TorcSetting acts as a wrapper around a persistent (stored in a database) or temporary
 * (system/state dependant) setting.
 *
 * Setting storage types are currently limited to Int, Bool, String or StringList but
 * could be extended to any QVariant supported type that can be converted to a string
 * and hence easily stored within the database.
 *
 * The setting type describes the setting's behaviour.
 *
 * TorcSetting additionally acts as a state machine. It can be activated by other settings
 * and QObjects and can notify other objects of any change to its value.
 *
 * A class can create and connect a number of TorcSettings to encapsulate the desired
 * setting behaviour and allow the setting structure to be presented and manipulated by
 * an appropriate user facing interface.
 *
 * \sa TorcUISetting
 */

TorcSetting::TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName,
                         Type SettingType, Roles SettingRoles, const QVariant &Default)
  : QObject(),
    TorcHTTPService(this, TORC_SETTINGS_DIR + DBName, (SettingRoles & Public) ? DBName : QStringLiteral(""),
                    TorcSetting::staticMetaObject, QStringLiteral("SetActive,SetTrue,SetFalse")),
    m_parent(Parent),
    type(SettingType),
    settingType(TorcCoreUtils::EnumToLowerString<TorcSetting::Type>(SettingType)),
    roles(SettingRoles),
    m_dbName(DBName),
    uiName(UIName),
    helpText(),
    value(),
    defaultValue(Default),
    selections(),
    m_begin(0),
    m_end(1),
    m_step(1),
    isActive(false),
    m_active(0),
    m_activeThreshold(1),
    m_children()
{
    setObjectName(DBName);

    qRegisterMetaType<QString>("QString&");

    if (m_parent)
        m_parent->AddChild(this);

    QVariant::Type vtype = defaultValue.type();

    if (vtype == QVariant::Int && type == Integer)
    {
        value = (roles & Persistent) ? gLocalContext->GetSetting(m_dbName, (int)defaultValue.toInt()) : defaultValue.toInt();
    }
    else if (vtype == QVariant::Bool && type == Bool)
    {
        value = (roles & Persistent) ? gLocalContext->GetSetting(m_dbName, (bool)defaultValue.toBool()) : defaultValue.toBool();
    }
    else if (vtype == QVariant::String)
    {
        value = (roles & Persistent) ? gLocalContext->GetSetting(m_dbName, (QString)defaultValue.toString()) : defaultValue.toString();
    }
    else if (vtype == QVariant::StringList)
    {
        if (roles & Persistent)
        {
            QString svalue = gLocalContext->GetSetting(m_dbName, (QString)defaultValue.toString());
            value = QVariant(svalue.split(','));
        }
        else
        {
            value = defaultValue.toStringList();
        }
    }
    else
    {
        if (vtype != QVariant::Invalid)
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unsupported setting data type for %1 (%2)").arg(m_dbName).arg(vtype));
        value = QVariant();
    }
}

void TorcSetting::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

QVariantMap TorcSetting::GetChildList(void)
{
    QReadLocker locker(&m_httpServiceLock);

    QVariantMap result;
    (void)GetChildList(result);
    return result;
}

QString TorcSetting::GetChildList(QVariantMap &Children)
{
    QReadLocker locker(&m_httpServiceLock);

    Children.insert(QStringLiteral("name"), m_dbName);
    Children.insert(QStringLiteral("uiname"), uiName);
    Children.insert(QStringLiteral("type"), TorcCoreUtils::EnumToLowerString<TorcSetting::Type>(type));
    QVariantMap children;
    foreach (TorcSetting* child, m_children)
    {
        QVariantMap childd;
        QString name = child->GetChildList(childd);
        children.insert(name, childd);
    }
    Children.insert(QStringLiteral("children"), children);
    return m_dbName;
}

QVariantMap TorcSetting::GetSelections(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return selections;
}

void TorcSetting::SetSelections(QVariantMap &Selections)
{
    QWriteLocker locker(&m_httpServiceLock);
    selections = Selections;
}

void TorcSetting::AddChild(TorcSetting *Child)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (Child)
    {
        m_children.append(Child);
        Child->UpRef();
    }
}
void TorcSetting::RemoveChild(TorcSetting *Child)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (Child)
    {
        {
            for (int i = 0; i < m_children.size(); ++i)
            {
                if (m_children.at(i) == Child)
                {
                    m_children.removeAt(i);
                    break;
                }
            }
        }

        Child->DownRef();
    }
}

void TorcSetting::Remove(void)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (m_parent)
        m_parent->RemoveChild(this);

    emit Removed();
}

TorcSetting* TorcSetting::FindChild(const QString &Child, bool Recursive /*=false*/)
{
    QReadLocker locker(&m_httpServiceLock);

    foreach (TorcSetting* setting, m_children)
        if (setting->objectName() == Child)
            return setting;

    if (Recursive)
    {
        foreach (TorcSetting* setting, m_children)
        {
            TorcSetting *result = setting->FindChild(Child, true);
            if (result)
                return result;
        }
    }

    return nullptr;
}

QSet<TorcSetting*> TorcSetting::GetChildren(void)
{
    QSet<TorcSetting*> result;

    m_httpServiceLock.lockForWrite();
    foreach (TorcSetting* setting, m_children)
    {
        result << setting;
        setting->UpRef();
    }
    m_httpServiceLock.unlock();

    return result;
}

bool TorcSetting::GetIsActive(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return isActive;
}

QString TorcSetting::GetUiName(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return uiName;
}

QString TorcSetting::GetHelpText(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return helpText;
}

QVariant TorcSetting::GetDefaultValue(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return defaultValue;
}

QString TorcSetting::GetSettingType(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return settingType;
}

int TorcSetting::GetBegin(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return m_begin;
}

int TorcSetting::GetEnd(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return m_end;
}

int TorcSetting::GetStep(void)
{
    QReadLocker locker(&m_httpServiceLock);
    return m_step;
}

void TorcSetting::SetActive(bool Value)
{
    m_httpServiceLock.lockForWrite();
    bool wasactive = isActive;
    m_active       += Value ? 1 : -1;
    isActive       = m_active >= m_activeThreshold;
    bool changed   = wasactive != isActive;
    bool newactive = isActive;
    m_httpServiceLock.unlock();

    if (changed)
        emit ActiveChanged(newactive);
}

void TorcSetting::SetActiveThreshold(int Threshold)
{
    m_httpServiceLock.lockForWrite();
    bool wasactive    = isActive;
    m_activeThreshold = Threshold;
    isActive          = m_active >= m_activeThreshold;
    bool changed      = wasactive != isActive;
    bool newactive    = isActive;
    m_httpServiceLock.unlock();

    if (changed)
        emit ActiveChanged(newactive);
}

bool TorcSetting::SetValue(const QVariant &Value)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (value == Value)
        return true;

    QVariant::Type vtype = defaultValue.type();
    if (vtype == QVariant::Int)
    {
        int ivalue = Value.toInt();
        bool valid = false;
        if (!selections.isEmpty())
        {
            valid = selections.contains(QString::number(ivalue));
        }
        else if (ivalue >= m_begin && ivalue <= m_end)
        {
            valid = true;
        }

        if (valid)
        {
            if (roles & Persistent)
                gLocalContext->SetSetting(m_dbName, (int)ivalue);
            value = Value;
            locker.unlock();
            emit ValueChanged(ivalue);
            return true;
        }
    }
    else if (vtype == QVariant::Bool)
    {
        bool bvalue = Value.toBool();
        if (roles & Persistent)
            gLocalContext->SetSetting(m_dbName, (bool)bvalue);
        value = Value;
        locker.unlock();
        emit ValueChanged(bvalue);
        return true;
    }
    else if (vtype == QVariant::String)
    {
        QString svalue = Value.toString();
        if (selections.isEmpty() ? true : selections.contains(svalue))
        {
            if (roles & Persistent)
                gLocalContext->SetSetting(m_dbName, svalue);
            value = Value;
            locker.unlock();
            emit ValueChanged(svalue);
            return true;
        }
    }
    else if (vtype == QVariant::StringList)
    {
        QStringList svalue = Value.toStringList();
        if (roles & Persistent)
            gLocalContext->SetSetting(m_dbName, svalue.join(','));
        value = Value;
        locker.unlock();
        emit ValueChanged(svalue);
        return true;
    }
    return false;
}

void TorcSetting::SetRange(int Begin, int End, int Step)
{
    QWriteLocker locker(&m_httpServiceLock);
    if (type != Integer)
        return;

    if (Begin >= End || Step < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Invalid setting range: begin %1 end %2 step %3")
            .arg(Begin).arg(End).arg(Step));
        return;
    }

    m_begin = Begin;
    m_end   = End;
    m_step  = Step;
}

void TorcSetting::SetHelpText(const QString &HelpText)
{
    QWriteLocker locker(&m_httpServiceLock);
    helpText = HelpText;
}

QVariant TorcSetting::GetValue(void)
{
    QReadLocker locker(&m_httpServiceLock);
    switch (type)
    {
        case Integer:    return value.toInt();
        case Bool:       return value.toBool();
        case String:     return value.toString();
        case StringList: return value.toStringList();
        case Group:      return QVariant();
    }

    return value;
}

/*! \class TorcSettingGroup
 *  \brief High level group of related settings
 *
 * \sa TorcSetting
*/

TorcSettingGroup::TorcSettingGroup(TorcSetting *Parent, const QString &UIName)
  : TorcSetting(Parent, UIName, UIName, Group, Public, QVariant())
{
    SetActiveThreshold(0);
}
