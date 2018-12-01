/* Class TorcControls
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

// Qt
#include <QMutex>

// Torc
#include "torclogging.h"
#include "torccoreutils.h"
#include "torclogiccontrol.h"
#include "torctimercontrol.h"
#include "torctransitioncontrol.h"
#include "torccontrols.h"

TorcControls* TorcControls::gControls = new TorcControls();

#define BLACKLIST QString("")

TorcControls::TorcControls()
  : QObject(),
    TorcHTTPService(this, CONTROLS_DIRECTORY, "controls", TorcControls::staticMetaObject, BLACKLIST),
    TorcDeviceHandler(),
    controlList(),
    controlTypes()
{
}

void TorcControls::Create(const QVariantMap &Details)
{
    QWriteLocker locker(&m_handlerLock);

    QVariantMap::const_iterator it = Details.constFind("controls");
    if (Details.constEnd() == it)
        return;

    QVariantMap control = it.value().toMap();
    it = control.constBegin();
    for ( ; it != control.constEnd(); ++it)
    {
        TorcControl::Type type = (TorcControl::Type)TorcCoreUtils::StringToEnum<TorcControl::Type>(it.key());
        if (type == TorcControl::Unknown)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown control type '%1'").arg(it.key()));
            continue;
        }

        QVariantMap controls = it.value().toMap();
        QVariantMap::const_iterator it2 = controls.constBegin();
        for ( ; it2 != controls.constEnd(); ++it2)
        {
            QVariantMap details = it2.value().toMap();
            if (!details.contains("name"))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Ignoring control type '%1' with no <name>").arg(TorcCoreUtils::EnumToLowerString<TorcControl::Type>(type)));
                continue;
            }

            switch (type)
            {
                case TorcControl::Logic:      controlList.append(new TorcLogicControl(it2.key(), details));
                case TorcControl::Timer:      controlList.append(new TorcTimerControl(it2.key(), details));
                case TorcControl::Transition: controlList.append(new TorcTransitionControl(it2.key(), details));
                default: break;
            }
        }
    }
}

void TorcControls::Destroy(void)
{
    QWriteLocker locker(&m_handlerLock);

    foreach (TorcControl *control, controlList)
        control->DownRef();
    controlList.clear();
}

void TorcControls::Validate(void)
{
    QWriteLocker locker(&m_handlerLock);

    // We first validate each control.

    // At this point all sensors, controls and outputs have been parsed successfully and
    // the objects created.
    // Validation checks each control has the correct number of valid inputs and outputs and then
    // connects the appropriate signal/slot pairs. At this point no signals should be triggered.

    QMutableListIterator<TorcControl*> it(controlList);
    while (it.hasNext())
    {
        TorcControl* control = it.next();
        if (!control->Validate())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to complete device '%1' - deleting").arg(control->GetUniqueId()));
            control->DownRef();
            it.remove();
        }
        else
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Registered control '%1'").arg(control->Name()));
        }
    }

    LOG(VB_GENERAL, LOG_DEBUG, "Controls validated");

    // Each control now has a complete list of inputs and outputs so we can now check for circular references.
    // N.B. Only controls are aware of other devices (i.e. they are created with the expectation that they are
    // connected to sensors/outputs/other controls. Sensors and outputs are notionally standalone.

    // Iterate over the controls list and recursively check for circular references.
    foreach (TorcControl* control, controlList)
    {
        QString id = control->GetUniqueId();
        QString path("");
        (void)control->CheckForCircularReferences(id, path);
    }
}

void TorcControls::Graph(QByteArray* Data)
{
    if (Data)
        foreach(TorcControl *control, controlList)
            control->Graph(Data);
}

QString TorcControls::GetUIName(void)
{
    return tr("Controls");
}

void TorcControls::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

QVariantMap TorcControls::GetControlList(void)
{
    QReadLocker locker(&m_handlerLock);

    QVariantMap result;
    // iterate over our list for each control type
    for (int type = TorcControl::Unknown; type < TorcControl::MaxType; type++)
    {
        QStringList controlsfortype;
        foreach (TorcControl *control, controlList)
            if (control->GetType() == type)
                controlsfortype.append(control->GetUniqueId());

        if (!controlsfortype.isEmpty())
            result.insert(TorcCoreUtils::EnumToLowerString<TorcControl::Type>(static_cast<TorcControl::Type>(type)), controlsfortype);
    }
    return result;

}

QStringList TorcControls::GetControlTypes(void) const
{
    QStringList result;
        for (int i = 0; i < TorcControl::MaxType; ++i)
            result << TorcCoreUtils::EnumToLowerString<TorcControl::Type>((TorcControl::Type)i);
    return result;

}

