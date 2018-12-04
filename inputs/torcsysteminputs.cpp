/* Class TorcSystemInputs
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

// Torc
#include "torclogging.h"
#include "torccoreutils.h"
#include "torcinputs.h"
#include "torcsysteminput.h"
#include "torcsysteminputs.h"

TorcSystemInputs* TorcSystemInputs::gSystemInputs = new TorcSystemInputs();

TorcSystemInputs::TorcSystemInputs()
  : TorcDeviceHandler(),
    m_inputs()
{
}

void TorcSystemInputs::Create(const QVariantMap &Details)
{
    QWriteLocker locker(&m_handlerLock);

    QVariantMap::const_iterator i = Details.begin();
    for ( ; i != Details.end(); ++i)
    {
        // network devices can be <sensors> or <outputs>
        if (i.key() != INPUTS_DIRECTORY)
            continue;

        QVariantMap devices = i.value().toMap();
        QVariantMap::const_iterator j = devices.constBegin();
        for ( ; j != devices.constEnd(); ++j)
        {
            // look for <network>
            if (j.key() != SYSTEM_INPUTS_STRING)
                continue;

            QVariantMap details = j.value().toMap();
            QVariantMap::const_iterator it = details.constBegin();
            for ( ; it != details.constEnd(); ++it)
            {
                // iterate over known types - currently only 'started'
                if (it.key() == TorcCoreUtils::EnumToLowerString<TorcInput::Type>(TorcInput::Started))
                {
                    QVariantMap system   = it.value().toMap();
                    QString uniqueid     = system.value("name").toString();
                    TorcSystemInput *input = new TorcSystemInput(0, system);
                    m_inputs.insert(uniqueid, input);
                }
            }
        }
    }
}

void TorcSystemInputs::Destroy(void)
{
    QWriteLocker locker(&m_handlerLock);

    QMap<QString,TorcInput*>::const_iterator it = m_inputs.constBegin();
    for ( ; it != m_inputs.constEnd(); ++it)
    {
        it.value()->DownRef();
        TorcInputs::gInputs->RemoveInput(it.value());
    }
    m_inputs.clear();
}
