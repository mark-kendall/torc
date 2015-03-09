/* Class TorcDevice
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
#include "torclogging.h"
#include "torccentral.h"
#include "torcdevice.h"

QHash<QString,QObject*>* TorcDevice::gDeviceList = new QHash<QString,QObject*>();
QMutex*      TorcDevice::gDeviceListLock         = new QMutex(QMutex::Recursive);

bool TorcDevice::UniqueIdAvailable(const QString &UniqueId)
{
    QMutexLocker locker(gDeviceListLock);

    return !UniqueId.isEmpty() && !gDeviceList->contains(UniqueId);
}

bool TorcDevice::RegisterUniqueId(const QString &UniqueId, QObject *Object)
{
    QMutexLocker locker(gDeviceListLock);

    if (UniqueIdAvailable(UniqueId) && Object)
    {
        gDeviceList->insert(UniqueId, Object);
        LOG(VB_GENERAL, LOG_INFO, QString("New device id: %1").arg(UniqueId));
        return true;
    }

    return false;
}

void TorcDevice::UnregisterUniqueId(const QString &UniqueId)
{
    QMutexLocker locker(gDeviceListLock);

    LOG(VB_GENERAL, LOG_INFO, QString("Device id: %1 removed").arg(UniqueId));
    gDeviceList->remove(UniqueId);
}

QObject* TorcDevice::GetObjectforId(const QString &UniqueId)
{
    QMutexLocker locker(gDeviceListLock);

    if (gDeviceList->contains(UniqueId))
        return gDeviceList->value(UniqueId);
    return NULL;
}

TorcDevice::TorcDevice(bool Valid, double Value, double Default,
                       const QString &ModelId, const QString &UniqueId,
                       const QVariantMap &Details)
  : TorcReferenceCounter(),
    valid(Valid),
    value(Value),
    defaultValue(Default),
    modelId(ModelId),
    uniqueId(UniqueId),
    userName(QString("")),
    userDescription(QString("")),
    lock(new QMutex(QMutex::Recursive))
{
    if (!TorcDevice::RegisterUniqueId(uniqueId, this))
        LOG(VB_GENERAL, LOG_ERR, QString("Device id '%1' already in use - THIS WILL NOT WORK").arg(uniqueId));

    if (Details.contains("userName"))
        SetUserName(Details.value("userName").toString());
    if (Details.contains("userDescription"))
        SetUserDescription(Details.value("userDescription").toString());
}

TorcDevice::~TorcDevice()
{
    TorcDevice::UnregisterUniqueId(uniqueId);
    delete lock;
    lock = NULL;
}

void TorcDevice::SetValid(bool Valid)
{
    QMutexLocker locker(lock);

    if (Valid == valid)
        return;

    valid = Valid;
    emit ValidChanged(valid);
}

void TorcDevice::SetValue(double Value)
{
    QMutexLocker locker(lock);

    // ignore same value updates
    if (qFuzzyCompare(Value + 1.0f, value + 1.0f))
        return;

    value = Value;
    emit ValueChanged(value);
}

void TorcDevice::SetUserName(const QString &Name)
{
    QMutexLocker locker(lock);

    if (Name == userName)
        return;

    userName = Name;
    emit UserNameChanged(userName);
}

void TorcDevice::SetUserDescription(const QString &Description)
{
    QMutexLocker locker(lock);

    if (Description == userDescription)
        return;

    userDescription = Description;
    emit UserDescriptionChanged(userDescription);
}

bool TorcDevice::GetValid(void)
{
    QMutexLocker locker(lock);

    return valid;
}

double TorcDevice::GetValue(void)
{
    QMutexLocker locker(lock);

    return value;
}

QString TorcDevice::GetModelId(void)
{
    // no need to lock
    return modelId;
}

QString TorcDevice::GetUniqueId(void)
{
    // no need to lock
    return uniqueId;
}

QString TorcDevice::GetUserName(void)
{
    QMutexLocker locker(lock);

    return userName;
}

QString TorcDevice::GetUserDescription(void)
{
    QMutexLocker locker(lock);

    return userDescription;
}

QList<TorcDeviceHandler*> TorcDeviceHandler::gTorcDeviceHandlers;
TorcDeviceHandler* TorcDeviceHandler::gTorcDeviceHandler = NULL;
QMutex* TorcDeviceHandler::gTorcDeviceHandlersLock = new QMutex(QMutex::Recursive);

TorcDeviceHandler::TorcDeviceHandler()
  : m_lock(new QMutex(QMutex::Recursive))
{
    QMutexLocker lock(gTorcDeviceHandlersLock);
    m_nextTorcDeviceHandler = gTorcDeviceHandler;
    gTorcDeviceHandler = this;
}

TorcDeviceHandler::~TorcDeviceHandler()
{
    delete m_lock;
    m_lock = NULL;
}

TorcDeviceHandler* TorcDeviceHandler::GetNextHandler(void)
{
    return m_nextTorcDeviceHandler;
}

TorcDeviceHandler* TorcDeviceHandler::GetDeviceHandler(void)
{
    return gTorcDeviceHandler;
}

void TorcDeviceHandler::Start(const QVariantMap &Details)
{
    TorcDeviceHandler* handler = TorcDeviceHandler::GetDeviceHandler();
    for ( ; handler; handler = handler->GetNextHandler())
        handler->Create(Details);
}

void TorcDeviceHandler::Stop(void)
{
    TorcDeviceHandler* handler = TorcDeviceHandler::GetDeviceHandler();
    for ( ; handler; handler = handler->GetNextHandler())
        handler->Destroy();
}

