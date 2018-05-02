/* Class TorcDevice
*
* Copyright (C) Mark Kendall 2015-18
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

QHash<QString,TorcDevice*>* TorcDevice::gDeviceList = new QHash<QString,TorcDevice*>();
QMutex*      TorcDevice::gDeviceListLock         = new QMutex(QMutex::Recursive);

TorcDevice::TorcDevice(bool Valid, double Value, double Default,
                       const QString &ModelId,   const QVariantMap &Details)
  : TorcReferenceCounter(),
    valid(Valid),
    value(Value),
    defaultValue(Default),
    modelId(ModelId),
    uniqueId(QString("")),
    userName(QString("")),
    userDescription(QString("")),
    lock(QMutex::Recursive),
    wasInvalid(true)
{
    if (!Details.contains("name"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Device has no <name> - THIS WILL NOT WORK"));
    }
    else
    {
        QMutexLocker locker(gDeviceListLock);
        uniqueId = Details.value("name").toString();
        setObjectName(uniqueId);

        if (!uniqueId.isEmpty() && !gDeviceList->contains(uniqueId))
        {
            gDeviceList->insert(uniqueId, this);
            LOG(VB_GENERAL, LOG_DEBUG, QString("New device id: %1").arg(uniqueId));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Device id '%1' already in use - THIS WILL NOT WORK").arg(uniqueId));
        }
    }

    if (Details.contains("username"))
        SetUserName(Details.value("username").toString());
    if (Details.contains("userdescription"))
        SetUserDescription(Details.value("userdescription").toString());
}

TorcDevice::~TorcDevice()
{
    {
        QMutexLocker locker(gDeviceListLock);

        LOG(VB_GENERAL, LOG_INFO, QString("Device id: %1 removed").arg(uniqueId));
        gDeviceList->remove(uniqueId);
    }
}

QStringList TorcDevice::GetDescription(void)
{
    return QStringList();
}

/*! \brief Start the device.
 *
 * The default implementation does nothing.
*/
void TorcDevice::Start(void)
{
}

/*! \brief Stop the device.
 *
 * Invalidate the device, which will also set its value to defaultValue.
*/
void TorcDevice::Stop(void)
{
    SetValid(false);
}

void TorcDevice::SetValid(bool Valid)
{
    QMutexLocker locker(&lock);

    if (Valid == valid)
        return;

    if (Valid)
        wasInvalid = true;

    valid = Valid;
    emit ValidChanged(valid);
}

void TorcDevice::SetValue(double Value)
{
    QMutexLocker locker(&lock);

    // force an update if the last value was 'invalid'
    if (wasInvalid)
    {
        wasInvalid = false;
    }
    else
    {
        // filter out unnecessary changes
        if (qFuzzyCompare(Value + 1.0f, value + 1.0f))
            return;
    }

    value = Value;
    emit ValueChanged(value);
}

void TorcDevice::SetUserName(const QString &Name)
{
    QMutexLocker locker(&lock);

    if (Name == userName)
        return;

    userName = Name;
    emit UserNameChanged(userName);
}

void TorcDevice::SetUserDescription(const QString &Description)
{
    QMutexLocker locker(&lock);

    if (Description == userDescription)
        return;

    userDescription = Description;
    emit UserDescriptionChanged(userDescription);
}

bool TorcDevice::GetValid(void)
{
    QMutexLocker locker(&lock);

    return valid;
}

double TorcDevice::GetValue(void)
{
    QMutexLocker locker(&lock);

    return value;
}

double TorcDevice::GetDefaultValue(void)
{
    QMutexLocker locker(&lock);

    return defaultValue;
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
    QMutexLocker locker(&lock);

    return userName;
}

QString TorcDevice::GetUserDescription(void)
{
    QMutexLocker locker(&lock);

    return userDescription;
}
