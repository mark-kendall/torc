/* Class TorcLogicControl
*
* Copyright (C) Mark Kendall 2015-16
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
#include "torcinput.h"
#include "torcoutput.h"
#include "torclogiccontrol.h"

TorcLogicControl::Operation TorcLogicControl::StringToOperation(const QString &Operation, bool *Ok)
{
    *Ok = true;
    QString operation = Operation.toUpper();

    if ("EQUAL" == operation)              return TorcLogicControl::Equal;
    if ("LESSTHAN" == operation)           return TorcLogicControl::LessThan;
    if ("LESSTHANOREQUAL" == operation)    return TorcLogicControl::LessThanOrEqual;
    if ("GREATERTHAN" == operation)        return TorcLogicControl::GreaterThan;
    if ("GREATERTHANOREQUAL" == operation) return TorcLogicControl::GreaterThanOrEqual;
    if ("ANY" == operation)                return TorcLogicControl::Any;
    if ("ALL" == operation)                return TorcLogicControl::All;
    if ("NONE" == operation)               return TorcLogicControl::None;
    if ("AVERAGE" == operation)            return TorcLogicControl::Average;
    if ("PASSTHROUGH" == operation)        return TorcLogicControl::Passthrough;
    if ("TOGGLE" == operation)             return TorcLogicControl::Toggle;
    if ("INVERT" == operation)             return TorcLogicControl::Invert;
    if ("MAXIMUM" == operation)            return TorcLogicControl::Maximum;
    if ("MINIMUM" == operation)            return TorcLogicControl::Minimum;
    if ("MULTIPLY" == operation)           return TorcLogicControl::Multiply;

    // fail for anything that isn't explicitly a known operation
    *Ok = false;
    return TorcLogicControl::Passthrough;
}

TorcLogicControl::TorcLogicControl(const QString &Type, const QVariantMap &Details)
  : TorcControl(TorcControl::Logic, Details),
    m_operation(TorcLogicControl::Passthrough)
{
    bool operationok = false;
    m_operation      = TorcLogicControl::StringToOperation(Type, &operationok);

    if (!operationok)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unrecognised control operation '%1' for device '%2'").arg(Type).arg(uniqueId));
        return;
    }

    // these operations require a valid value to operate against
    if (m_operation == TorcLogicControl::Equal ||
        m_operation == TorcLogicControl::LessThan ||
        m_operation == TorcLogicControl::LessThanOrEqual ||
        m_operation == TorcLogicControl::GreaterThan ||
        m_operation == TorcLogicControl::GreaterThanOrEqual)
    {
        if (!Details.contains("references"))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no reference device for operation").arg(uniqueId));
            return;
        }

        QVariantMap reference = Details.value("references").toMap();
        if (!reference.contains("device"))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no reference device for operation").arg(uniqueId));
            return;
        }

        m_referenceDeviceId = reference.value("device").toString().trimmed();

        // and treat it as a normal input - this ensures TorcControl takes care of all of the input value and
        // valid logic. We just ensure we know which is the reference device/value.
        m_inputList.append(m_referenceDeviceId);
        m_inputList.removeDuplicates();
    }

    // everything appears to be valid at this stage
    m_parsed = true;
}

TorcLogicControl::~TorcLogicControl()
{
}

TorcControl::Type TorcLogicControl::GetType(void)
{
    return TorcControl::Logic;
}

QStringList TorcLogicControl::GetDescription(void)
{
    QStringList result;
    QString reference("Unknown");
    TorcDevice *device = qobject_cast<TorcDevice*>(m_referenceDevice);
    if (device)
        reference = device->GetUserName();

    switch (m_operation)
    {
        case TorcLogicControl::Equal:
            result.append(tr("Equal to '%1'").arg(reference));
            break;
        case TorcLogicControl::LessThan:
            result.append(tr("Less than '%1'").arg(reference));
            break;
        case TorcLogicControl::LessThanOrEqual:
            result.append(tr("Less than or equal to '%1'").arg(reference));
            break;
        case TorcLogicControl::GreaterThan:
            result.append(tr("Greater than '%1'").arg(reference));
            break;
        case TorcLogicControl::GreaterThanOrEqual:
            result.append(tr("Greater than or equal to '%1'").arg(reference));
            break;
        case TorcLogicControl::Any:
            result.append(tr("Any"));
            break;
        case TorcLogicControl::All:
            result.append(tr("All"));
            break;
        case TorcLogicControl::None:
            result.append(tr("None"));
            break;
        case TorcLogicControl::Average:
            result.append(tr("Average"));
            break;
        case TorcLogicControl::Toggle:
            result.append(tr("Toggle"));
            break;
        case TorcLogicControl::Invert:
            result.append(tr("Invert"));
            break;
        case TorcLogicControl::Passthrough:
            result.append(tr("Passthrough"));
            break;
        case TorcLogicControl::Maximum:
            result.append(tr("Maximum"));
            break;
        case TorcLogicControl::Minimum:
            result.append(tr("Minimum"));
            break;
        case TorcLogicControl::Multiply:
            result.append(tr("Multiply"));
            break;
    }

    return result;
}

bool TorcLogicControl::IsPassthrough(void)
{
    QMutexLocker locker(lock);

    bool passthrough = false;
    if ((m_operation == TorcLogicControl::Passthrough) && (m_inputs.size() == 1))
    {
        // check the input
        if (qobject_cast<TorcInput*>(m_inputs.firstKey()))
        {
            // and the outputs
            passthrough = true;
            QMap<QObject*,QString>::const_iterator it = m_outputs.constBegin();
            for ( ; it != m_outputs.constEnd(); ++it)
                passthrough &= (bool)qobject_cast<TorcOutput*>(it.key());
        }
    }

    return passthrough;
}

bool TorcLogicControl::Validate(void)
{
    QMutexLocker locker(lock);

    // don't repeat validation
    if (m_validated)
        return true;

    // common checks
    if (!TorcControl::Validate())
        return false;

    // reference device must be valid if TorcControl::Validate is happy
    m_referenceDevice = gDeviceList->value(m_referenceDeviceId);

    // we always need one or more outputs
    if (m_outputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Device '%1' needs at least one output").arg(uniqueId));
        return false;
    }

    switch (m_operation)
    {
        case TorcLogicControl::Any:
        case TorcLogicControl::All:
        case TorcLogicControl::None:
        case TorcLogicControl::Average:
        case TorcLogicControl::Maximum:
        case TorcLogicControl::Minimum:
        case TorcLogicControl::Multiply:
            {
                if (m_inputs.size() < 2)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (needs at least 2) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(",")));
                    return false;
                }
            }
            break;
        case TorcLogicControl::Passthrough:
        case TorcLogicControl::Toggle:
        case TorcLogicControl::Invert:
            {
                if (m_inputs.size() != 1)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (must have 1) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(",")));
                    return false;
                }
            }
            break;
        case TorcLogicControl::Equal:
        case TorcLogicControl::LessThan:
        case TorcLogicControl::LessThanOrEqual:
        case TorcLogicControl::GreaterThan:
        case TorcLogicControl::GreaterThanOrEqual:
            // these should have one defined input and one reference device that is handled as an input
            {
                if (m_inputs.size() != 2)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (must have 1 input and 1 reference) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(",")));
                    return false;
                }
            }
            break;
    }

    // if we get this far, we can finish the device
    if (!Finish())
        return false;

    return true;
}

void TorcLogicControl::CalculateOutput(void)
{
    QMutexLocker locker(lock);

    double newvalue = value; // no change by default

    double referencevalue = 0.0;
    double inputvalue     = 0.0;

    if (TorcLogicControl::Equal == m_operation ||
        TorcLogicControl::LessThan == m_operation ||
        TorcLogicControl::LessThanOrEqual == m_operation ||
        TorcLogicControl::GreaterThan == m_operation ||
        TorcLogicControl::GreaterThanOrEqual == m_operation)
    {
        if (m_inputValues.size() == 2)
        {
            if (m_inputValues.firstKey() == m_referenceDevice)
            {
                referencevalue = m_inputValues.first();
                inputvalue     = m_inputValues.last();
            }
            else
            {
                referencevalue = m_inputValues.last();
                inputvalue     = m_inputValues.first();
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid control input size"));
        }
    }

    switch (m_operation)
    {
        case TorcLogicControl::Passthrough:
            newvalue = m_inputValues.first();
            break;
        case TorcLogicControl::Multiply:
            {
                // must be multiple range/pwm values that are combined
                // if binary inputs are used, this will equate to the opposite of Any.
                double start = 1.0;
                foreach(double next, m_inputValues)
                    start *= next;
                newvalue = start;
            }
            break;
        case TorcLogicControl::Equal:
            // single value ==
            newvalue = qFuzzyCompare(inputvalue + 1.0, referencevalue + 1.0) ? 1 : 0;
            break;
        case TorcLogicControl::LessThan:
            // single value <
            newvalue = inputvalue < referencevalue ? 1 : 0;
            break;
        case TorcLogicControl::LessThanOrEqual:
            // Use qFuzzyCompare for = ?
            // single input <=
            newvalue = inputvalue <= referencevalue ? 1 : 0;
            break;
        case TorcLogicControl::GreaterThan:
            // single input >
            newvalue = inputvalue > referencevalue ? 1 : 0;
            break;
        case TorcLogicControl::GreaterThanOrEqual:
            // single input >=
            newvalue = inputvalue >= referencevalue ? 1 : 0;
            break;
        case TorcLogicControl::All:
            {
                // multiple binary (on/off) inputs that must all be 1/On/non-zero
                bool on = true;
                foreach (double next, m_inputValues)
                    on &= !qFuzzyCompare(next + 1.0, 1.0);
                newvalue = on ? 1 : 0;
            }
            break;
        case TorcLogicControl::Any:
        case TorcLogicControl::None:
            {
                // multiple binary (on/off) inputs
                bool on = false;
                foreach (double next, m_inputValues)
                    on |= !qFuzzyCompare(next + 1.0, 1.0);
                if (m_operation == TorcLogicControl::Any)
                    newvalue = on ? 1 : 0;
                else
                    newvalue = on ? 0 : 1;
            }
            break;
        case TorcLogicControl::Average:
            {
                // does exactly what it says on the tin
                double average = 0;
                foreach (double next, m_inputValues)
                    average += next;
                newvalue = average / m_inputValues.size();
            }
            break;
        case TorcLogicControl::Toggle:
            {
                // the output is toggled for every 'rising' input (i.e. when the input changes from
                // a value that is less than 1 to a value that is greater than or equal to 1
                if (m_lastInputValues.first() < 1.0 && m_inputValues.first() >= 1.0)
                    newvalue = value >= 1.0 ? 0.0 : 1.0;
            }
            break;
        case TorcLogicControl::Invert:
            {
                newvalue = m_inputValues.first() < 1.0 ? 1.0 : 0.0;
            }
            break;
        case TorcLogicControl::Maximum:
            {
                double max = 0;
                foreach (double next, m_inputValues)
                    if (next > max)
                        max = next;
                newvalue = max;
            }
            break;
        case TorcLogicControl::Minimum:
            {
                double min = qInf();
                foreach (double next, m_inputValues)
                    if (next < min)
                        min = next;
                newvalue = min;
            }
            break;
    }
    SetValue(newvalue);
}
