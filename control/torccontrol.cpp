/* Class TorcControl
*
* Copyright (C) Mark Kendall 2015
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Torc
#include "torccontrol.h"

TorcControl::TorcControl(const QVariantMap &Details)
  : QObject(),
    TorcDevice(false, 0, 0, QString(""), QString("")),
    m_operation(TorcControl::None),
    m_operationValue(0)
{
    (void)Details;
}

TorcControl::~TorcControl()
{
}

void TorcControl::InputValueChanged(double Value)
{
    QMutexLocker locker(lock);

    (void)Value;
}

void TorcControl::InputValidChanged(bool Valid)
{
    QMutexLocker locker(lock);

    (void)Valid;
}

bool TorcControl::GetValid(void)
{
    QMutexLocker locker(lock);
    return valid;
}

double TorcControl::GetValue(void)
{
    QMutexLocker locker(lock);
    return value;
}

QString TorcControl::GetUniqueId(void)
{
    QMutexLocker locker(lock);
    return uniqueId;
}

QString TorcControl::GetUserName(void)
{
    QMutexLocker locker(lock);
    return userName;
}

QString TorcControl::GetUserDescription(void)
{
    QMutexLocker locker(lock);
    return userDescription;
}

void TorcControl::SetUserName(const QString &Name)
{
    QMutexLocker locker(lock);

    if (Name == userName)
        return;

    userName = Name;
    emit UserNameChanged(userName);
}

void TorcControl::SetUserDescription(const QString &Description)
{
    QMutexLocker locker(lock);

    if (Description == userDescription)
        return;

    userDescription = Description;
    emit UserDescriptionChanged(userDescription);
}

void TorcControl::SetValid(bool Valid)
{
    QMutexLocker locker(lock);

    if (valid == Valid)
        return;

    valid = Valid;
    emit ValidChanged(valid);
}
