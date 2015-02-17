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
        if (it.key() != "controls")
            continue;

        QVariantMap controls      = it.value().toMap();
        QVariantMap::iterator it2 = controls.begin();
        for ( ; it2 != controls.end(); ++it2)
        {
            QString id = it2.key();

            if (id.isEmpty())
                continue;

            id = "controls_" + id;

            if (!TorcDevice::UniqueIdAvailable(id))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Device id '%1' already in use - ignoring").arg(id));
                continue;
            }

            QVariantMap details = it2.value().toMap();
            QString name  = details.value("userName").toString();
            QString desc  = details.value("userDescription").toString();
            QString op    = details.value("operation").toString();
            QString val   = details.value("value").toString();

            QStringList inputs = details.value("inputs").toStringList();
            inputs.removeDuplicates();
            inputs.removeAll("");

            if (inputs.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no inputs - ignoring").arg(id));
                continue;
            }

            QStringList outputs = details.value("outputs").toStringList();
            outputs.removeDuplicates();
            outputs.removeAll("");

            if (outputs.isEmpty())
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no outputs - ignoring").arg(id));
                continue;
            }

            bool operational = false;
            bool ok = false;
            TorcControl::Operation operation = TorcControl::StringToOperation(op);
            double operationvalue = val.toDouble(&ok);

            if (operation == TorcControl::Equal ||
                operation == TorcControl::LessThan ||
                operation == TorcControl::LessThanOrEqual ||
                operation == TorcControl::GreaterThan ||
                operation == TorcControl::GreaterThanOrEqual)
            {
                // a control that makes a decision needs one input and a valid value to compare
                // that input to
                // TODO somewhere need to validate the value against the input type
                // e.g. no point in having a value of 10 for a pwm input (0-1)
                if (!details.contains("value"))
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no value for operation - ignoring").arg(id));
                    continue;
                }

                if (!ok)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse value for control '%1' - ignoring").arg(id));
                    continue;
                }

                if (inputs.size() > 1)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has more than 1 input for operation - ignoring").arg(id));
                    continue;
                }

                // all is good
                operational = true;
            }


            LOG(VB_GENERAL, LOG_INFO, QString("%1 Inputs   : %2").arg(id).arg(inputs.join(",")));
            LOG(VB_GENERAL, LOG_INFO, QString("%1 Outputs  : %2").arg(id).arg(outputs.join(",")));
            if (operational)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("%1 Operation: Input %2 - %3 - %4")
                    .arg(id).arg(inputs[0]).arg(TorcControl::OperationToString(operation)).arg(operationvalue));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, QString("%1 Operation: %2")
                    .arg(id).arg(TorcControl::OperationToString(operation)));
            }

            TorcControl* control = new TorcControl(id, inputs, outputs, operation, operationvalue);
            control->SetUserName(name);
            control->SetUserDescription(desc);
            m_controls.insert(id, control);
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

    foreach(TorcControl* control, m_controls)
        control->Validate();
}
