/* Class TorcControls
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

// Qt
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torccontrols.h"

TorcControls* TorcControls::gControls = new TorcControls();

TorcControls::TorcControls()
  : TorcDeviceHandler()
{
}

TorcControls::~TorcControls()
{
}

void TorcControls::Create(const QVariantMap &Details)
{
    QMutexLocker locker(m_lock);

    QVariantMap::const_iterator it = Details.constBegin();
    for( ; it != Details.constEnd(); ++it)
    {
        if (it.key() != "control")
            continue;

        QVariantMap control       = it.value().toMap();
        QVariantMap::iterator it2 = control.begin();
        for ( ; it2 != control.end(); ++it2)
        {
            TorcControl::Type type = TorcControl::StringToType(it2.key());

            // logic, timer or transition
            if (type == TorcControl::UnknownType)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Unknown control type '%1'").arg(it2.key()));
                continue;
            }

            QVariantMap controls = it2.value().toMap();
            QVariantMap::iterator it3 = controls.begin();
            for ( ; it3 != controls.end(); ++it3)
            {
                QString id = QString("control_%1_%2").arg(TorcControl::TypeToString(type)).arg(it3.key());

                if (!TorcDevice::UniqueIdAvailable(id))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Device id '%1' already in use - ignoring").arg(id));
                    continue;
                }

                QVariantMap details = it3.value().toMap();
                TorcControl* control = new TorcControl(type, id, details);
                m_controls.insert(id, control);
            }
        }
    }
}

void TorcControls::Destroy(void)
{
    QMutexLocker locker(m_lock);

    QMap<QString, TorcControl*>::iterator it = m_controls.begin();
    for ( ; it != m_controls.end(); ++it)
        it.value()->DownRef();
    m_controls.clear();
}

void TorcControls::Validate(void)
{
    QMutexLocker locker(m_lock);

    QMutableMapIterator<QString,TorcControl*> it(m_controls);
    while (it.hasNext())
    {
        it.next();
        if (!it.value()->Validate())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to complete device '%1' creation - deleting").arg(it.key()));
            it.value()->DownRef();
            m_controls.remove(it.key());
        }
    }
}

void TorcControls::Graph(void)
{
    // currently unused as not adding a control cluster helps dot draw the graph and hides
    // 'passthrough' controls
}
