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
#include "torcdevice.h"

TorcDevice::TorcDevice(bool Valid, double Value, double Default,
                       const QString &ModelId, const QString &UniqueId,
                       const QString &UserName, const QString &userDescription)
  : TorcReferenceCounter(),
    valid(Valid),
    value(Value),
    defaultValue(Default),
    modelId(ModelId),
    uniqueId(UniqueId),
    userName(UserName),
    userDescription(userDescription),
    lock(new QMutex(QMutex::Recursive))
{
}

TorcDevice::~TorcDevice()
{
    delete lock;
    lock = NULL;
}
