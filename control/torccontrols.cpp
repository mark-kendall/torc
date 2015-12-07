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
#include "torclogiccontrol.h"
#include "torctimercontrol.h"
#include "torctransitioncontrol.h"
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
                QVariantMap details = it3.value().toMap();

                if (!details.contains("name"))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Ignoring control type '%1' with no <name>").arg(TorcControl::TypeToString(type)));
                    continue;
                }

                TorcControl* control = NULL;
                if (type == TorcControl::Logic)
                    control = new TorcLogicControl(it3.key(), details);
                else if (type == TorcControl::Timer)
                    control = new TorcTimerControl(it3.key(), details);
                else
                    control = new TorcTransitionControl(it3.key(), details);
                m_controls.append(control);
            }
        }
    }
}

void TorcControls::Destroy(void)
{
    QMutexLocker locker(m_lock);

    foreach (TorcControl *control, m_controls)
        control->DownRef();
    m_controls.clear();
}

void TorcControls::Validate(void)
{
    QMutexLocker locker(m_lock);

    // We first validate each control.

    // At this point all sensors, controls and outputs have been parsed successfully and
    // the objects created.
    // Validation checks each control has the correct number of valid inputs and outputs and then
    // connects the appropriate signal/slot pairs. At this point no signals should be triggered.

    QMutableListIterator<TorcControl*> it(m_controls);
    while (it.hasNext())
    {
        TorcControl* control = it.next();
        if (!control->Validate())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to complete device '%1' - deleting").arg(control->GetUniqueId()));
            control->DownRef();
            it.remove();
        }
    }

    LOG(VB_GENERAL, LOG_INFO, "Controls validated");

    // Each control now has a complete list of inputs and outputs so we can now check for circular references.
    // N.B. Only controls are aware of other devices (i.e. they are created with the expectation that they are
    // connected to sensors/outputs/other controls. Sensors and outputs are notionally standalone.

    // Iterate over the controls list and recursively check for circular references.
    foreach (TorcControl* control, m_controls)
    {
        QString id = control->GetUniqueId();
        QString path("");
        (void)control->CheckForCircularReferences(id, path);
    }
}

void TorcControls::Graph(void)
{
    // currently unused as not adding a control cluster helps dot draw the graph and hides
    // 'passthrough' controls
}

void TorcControls::Start(void)
{
    QMutexLocker locker(m_lock);

    foreach (TorcControl *control, m_controls)
        control->Start();
}

void TorcControls::Reset(void)
{
    QMutexLocker locker(m_lock);

    foreach (TorcControl *control, m_controls)
        control->Reset();
}
