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

QString TorcSetting::TypeToString(Type type)
{
    switch (type)
    {
        case Bool:       return QString("bool");
        case Integer:    return QString("integer");
        case String:     return QString("string");
        case StringList: return QString("stringlist");
    }
    return QString("erRor");
}

TorcSetting::TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName,
                         Type SettingType, bool Persistent, const QVariant &Default)
  : QObject(),
    TorcHTTPService(this, "settings/" + DBName, DBName, TorcSetting::staticMetaObject, "SetActive,SetTrue,SetFalse"),
    m_parent(Parent),
    type(SettingType),
    settingType(TypeToString(SettingType)),
    persistent(Persistent),
    m_dbName(DBName),
    uiName(UIName),
    description(),
    helpText(),
    value(),
    defaultValue(Default),
    m_begin(0),
    m_end(1),
    m_step(1),
    isActive(false),
    m_active(0),
    m_activeThreshold(1),
    m_children(),
    m_childrenLock(new QMutex(QMutex::Recursive))
{
    setObjectName(DBName);

    if (m_parent)
        m_parent->AddChild(this);

    QVariant::Type vtype = defaultValue.type();

    if (vtype == QVariant::Int && type == Integer)
    {
        value = persistent ? gLocalContext->GetSetting(m_dbName, (int)defaultValue.toInt()) : defaultValue.toInt();
    }
    else if (vtype == QVariant::Bool && type == Bool)
    {
        value = persistent ? gLocalContext->GetSetting(m_dbName, (bool)defaultValue.toBool()) : defaultValue.toBool();
    }
    else if (vtype == QVariant::String)
    {
        value = persistent ? gLocalContext->GetSetting(m_dbName, (QString)defaultValue.toString()) : defaultValue.toString();
    }
    else if (vtype == QVariant::StringList)
    {
        if (persistent)
        {
            QString svalue = gLocalContext->GetSetting(m_dbName, (QString)defaultValue.toString());
            value = QVariant(svalue.split(","));
        }
        else
        {
            value = defaultValue.toStringList();
        }
    }
    else
    {
        if (vtype != QVariant::Invalid)
            LOG(VB_GENERAL, LOG_ERR, QString("Unsupported setting data type for %1 (%2)").arg(m_dbName).arg(vtype));
        value = QVariant();
    }
}

TorcSetting::~TorcSetting()
{
    delete m_childrenLock;
}

void TorcSetting::SubscriberDeleted(QObject *Subscriber)
{
    return TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

QVariant::Type TorcSetting::GetStorageType(void)
{
    return defaultValue.type();
}

void TorcSetting::AddChild(TorcSetting *Child)
{
    if (Child)
    {
        {
            QMutexLocker locker(m_childrenLock);

            int position = m_children.size();
            m_children.insert(position, Child);
        }

        Child->UpRef();
    }
}
void TorcSetting::RemoveChild(TorcSetting *Child)
{
    if (Child)
    {
        {
            QMutexLocker locker(m_childrenLock);

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
    if (m_parent)
        m_parent->RemoveChild(this);

    emit Removed();
}

TorcSetting* TorcSetting::FindChild(const QString &Child, bool Recursive /*=false*/)
{
    QMutexLocker locker(m_childrenLock);

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

    return NULL;
}

QSet<TorcSetting*> TorcSetting::GetChildren(void)
{
    QSet<TorcSetting*> result;

    m_childrenLock->lock();
    foreach (TorcSetting* setting, m_children)
    {
        result << setting;
        setting->UpRef();
    }
    m_childrenLock->unlock();

    return result;
}

bool TorcSetting::GetIsActive(void)
{
    return isActive;
}

QString TorcSetting::GetUiName(void)
{
    return uiName;
}

QString TorcSetting::GetDescription(void)
{
    return description;
}

QString TorcSetting::GetHelpText(void)
{
    return helpText;
}

QVariant TorcSetting::GetDefaultValue(void)
{
    return defaultValue;
}

bool TorcSetting::GetPersistent(void)
{
    return persistent;
}

QString TorcSetting::GetSettingType(void)
{
    return settingType;
}

TorcSetting::Type TorcSetting::GetType(void)
{
    return type;
}

TorcSetting* TorcSetting::GetChildByIndex(int Index)
{
    // TODO does this need locking
    if (Index < 0 || Index >= m_children.size())
        return NULL;

    return m_children.at(Index);
}

int TorcSetting::GetBegin(void)
{
    return m_begin;
}

int TorcSetting::GetEnd(void)
{
    return m_end;
}

int TorcSetting::GetStep(void)
{
    return m_step;
}

void TorcSetting::SetActive(bool Value)
{
    bool wasactive = isActive;
    m_active += Value ? 1 : -1;
    isActive = m_active >= m_activeThreshold;

    if (wasactive != isActive)
        emit ActiveChanged(isActive);
}

void TorcSetting::SetActiveThreshold(int Threshold)
{
    bool wasactive = isActive;
    m_activeThreshold = Threshold;
    isActive = m_active >= m_activeThreshold;

    if (wasactive != isActive)
        emit ActiveChanged(isActive);
}

void TorcSetting::SetTrue(void)
{
    if (defaultValue.type() == QVariant::Bool)
        SetValue(QVariant((bool)true));
}

void TorcSetting::SetFalse(void)
{
    if (defaultValue.type() == QVariant::Bool)
        SetValue(QVariant((bool)false));
}

void TorcSetting::SetValue(const QVariant &Value)
{
    if (value == Value)
        return;

    value = Value;

    QVariant::Type vtype = defaultValue.type();

    if (vtype == QVariant::Int)
    {
        int ivalue = value.toInt();
        if (ivalue >= m_begin && ivalue <= m_end)
        {
            if (persistent)
                gLocalContext->SetSetting(m_dbName, (int)ivalue);
            emit ValueChanged(ivalue);
        }
    }
    else if (vtype == QVariant::Bool)
    {
        bool bvalue = value.toBool();
        if (persistent)
            gLocalContext->SetSetting(m_dbName, (bool)bvalue);

        emit ValueChanged(bvalue);
    }
    else if (vtype == QVariant::String)
    {
        QString svalue = value.toString();
        if (persistent)
            gLocalContext->SetSetting(m_dbName, svalue);
        emit ValueChanged(svalue);
    }
    else if (vtype == QVariant::StringList)
    {
        QStringList svalue = value.toStringList();
        if (persistent)
            gLocalContext->SetSetting(m_dbName, svalue.join(","));
        emit ValueChanged(svalue);
    }
}

void TorcSetting::SetRange(int Begin, int End, int Step)
{
    if (type != Integer)
        return;

    if (Begin >= End || Step < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid setting range: begin %1 end %2 step %3")
            .arg(Begin).arg(End).arg(Step));
        return;
    }

    m_begin = Begin;
    m_end   = End;
    m_step  = Step;
}

void TorcSetting::SetDescription(const QString &Description)
{
    description = Description;
}

void TorcSetting::SetHelpText(const QString &HelpText)
{
    helpText = HelpText;
}

QVariant TorcSetting::GetValue(void)
{
    switch (type)
    {
        case Integer:    return value.toInt();
        case Bool:       return value.toBool();
        case String:     return value.toString();
        case StringList: return value.toStringList();
    }

    return value;
}

/*! \class TorcSettingGroup
 *  \brief High level group of related settings
 *
 * \sa TorcSetting
*/

TorcSettingGroup::TorcSettingGroup(TorcSetting *Parent, const QString &UIName)
  : TorcSetting(Parent, UIName, UIName, Bool, false, QVariant())
{
    SetActiveThreshold(0);
}
