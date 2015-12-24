/* Class TorcLogicControl
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
#include "torcsensor.h"
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
    if ("AVERAGE" == operation)            return TorcLogicControl::Average;
    if ("NONE" == operation)               return TorcLogicControl::NoOperation;
    if ("PASSTHROUGH" == operation)        return TorcLogicControl::NoOperation;
    if ("TOGGLE" == operation)             return TorcLogicControl::Toggle;

    // fail for anything that isn't explicitly a known operation
    *Ok = false;
    return TorcLogicControl::NoOperation;
}

TorcLogicControl::TorcLogicControl(const QString &Type, const QVariantMap &Details)
  : TorcControl(Details),
    m_operation(TorcLogicControl::NoOperation),
    m_operationValue(0.0)
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
        // a value is explicitly required rather than defaulting to 0
        if (!Details.contains("value"))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no value for operation").arg(uniqueId));
            return;
        }

        QString val   = Details.value("value").toString();
        bool valueok = false;
        m_operationValue = val.toDouble(&valueok);

        // failed to parse value
        if (!valueok)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse value '%1' for device '%2'").arg(val).arg(uniqueId));
            return;
        }
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

    switch (m_operation)
    {
        case TorcLogicControl::Equal:
            result.append(tr("Equal to %1").arg(m_operationValue));
            break;
        case TorcLogicControl::LessThan:
            result.append(tr("Less than %1").arg(m_operationValue));
            break;
        case TorcLogicControl::LessThanOrEqual:
            result.append(tr("Less than or equal to %1").arg(m_operationValue));
            break;
        case TorcLogicControl::GreaterThan:
            result.append(tr("Greater than %1").arg(m_operationValue));
            break;
        case TorcLogicControl::GreaterThanOrEqual:
            result.append(tr("Greater than or equal to %1").arg(m_operationValue));
            break;
        case TorcLogicControl::Any:
            result.append(tr("Any"));
            break;
        case TorcLogicControl::All:
            result.append(tr("All"));
            break;
        case TorcLogicControl::Average:
            result.append(tr("Average"));
            break;
        case TorcLogicControl::Toggle:
            result.append(tr("Toggle"));
            break;
        case TorcLogicControl::NoOperation:
        default:
            result.append(tr("Passthrough"));
            break;
    }

    return result;
}

bool TorcLogicControl::IsPassthrough(void)
{
    QMutexLocker locker(lock);

    bool passthrough = false;
    if ((m_operation == TorcLogicControl::NoOperation) && (m_inputs.size() == 1))
    {
        // check the input
        if (qobject_cast<TorcSensor*>(m_inputs.firstKey()))
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

    // we need one or more inputs for a logic control
    if (m_inputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Device '%1' needs at least one input").arg(uniqueId));
        return false;
    }

    // we always need one or more outputs
    if (m_outputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Device '%1' needs at least one output").arg(uniqueId));
        return false;
    }

    // sanity check number of inputs for operation type
    if (m_operation == TorcLogicControl::Equal ||
        m_operation == TorcLogicControl::LessThan ||
        m_operation == TorcLogicControl::LessThanOrEqual ||
        m_operation == TorcLogicControl::GreaterThan ||
        m_operation == TorcLogicControl::GreaterThanOrEqual ||
        m_operation == TorcLogicControl::Toggle)
    {
        // can have only one input to compare against
        if (m_inputs.size() > 1)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (can have only 1) - ignoring.")
                .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(",")));
            return false;
        }
    }
    else if (m_operation == TorcLogicControl::Any ||
             m_operation == TorcLogicControl::All ||
             m_operation == TorcLogicControl::Average)
    {
        // ANY, ALL and AVERAGE imply multiple inputs. Technically they will operate fine with just one
        // but fail and warn - the user needs to know as their config may not be correct
        if (m_inputs.size() < 2)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (needs at least 2) - ignoring.")
                .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(",")));
            return false;
        }
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
    switch (m_operation)
    {
        case TorcLogicControl::NoOperation:
            if (m_inputs.size() == 1)
            {
                // must be a single input just passed through to output(s)
                newvalue = m_inputValues.first();
            }
            else
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
            newvalue = qFuzzyCompare(m_inputValues.first() + 1.0, m_operationValue + 1.0) ? 1 : 0;
            break;
        case TorcLogicControl::LessThan:
            // single value <
            newvalue = m_inputValues.first() < m_operationValue ? 1 : 0;
            break;
        case TorcLogicControl::LessThanOrEqual:
            // Use qFuzzyCompare for = ?
            // single input <=
            newvalue = m_inputValues.first() <= m_operationValue ? 1 : 0;
            break;
        case TorcLogicControl::GreaterThan:
            // single input >
            newvalue = m_inputValues.first() > m_operationValue ? 1 : 0;
            break;
        case TorcLogicControl::GreaterThanOrEqual:
            // single input >=
            newvalue = m_inputValues.first() >= m_operationValue ? 1 : 0;
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
            {
                // multiple binary (on/off) inputs - one or more of which must be 1/On/non-zero
                bool on = false;
                foreach (double next, m_inputValues)
                    on |= !qFuzzyCompare(next + 1.0, 1.0);
                newvalue = on ? 1 : 0;
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
    }
    SetValue(newvalue);
}
