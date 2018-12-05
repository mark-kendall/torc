/* Class TorcLogicControl
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
#include <QtGlobal>

// Torc
#include "torclogging.h"
#include "torcinput.h"
#include "torcoutput.h"
#include "torclogiccontrol.h"

TorcLogicControl::Operation TorcLogicControl::StringToOperation(const QString &Operation)
{
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
    if ("RUNNINGAVERAGE" == operation)     return TorcLogicControl::RunningAverage;
    if ("RUNNINGMAX" == operation)         return TorcLogicControl::RunningMax;
    if ("RUNNINGMIN" == operation)         return TorcLogicControl::RunningMin;


    return TorcLogicControl::UnknownLogicType;
}

inline bool IsComplexType(TorcLogicControl::Operation Type)
{
    if (Type == TorcLogicControl::Equal ||
        Type == TorcLogicControl::LessThan ||
        Type == TorcLogicControl::LessThanOrEqual ||
        Type == TorcLogicControl::GreaterThan ||
        Type == TorcLogicControl::GreaterThanOrEqual ||
        Type == TorcLogicControl::RunningAverage ||
        Type == TorcLogicControl::RunningMax ||
        Type == TorcLogicControl::RunningMin)
    {
        return true;
    }

    return false;
}

TorcLogicControl::TorcLogicControl(const QString &Type, const QVariantMap &Details)
  : TorcControl(TorcControl::Logic, Details),
    m_operation(TorcLogicControl::StringToOperation(Type)),
    m_referenceDeviceId(),
    m_referenceDevice(nullptr),
    m_inputDevice(nullptr),
    m_triggerDeviceId(),
    m_triggerDevice(nullptr),
    m_average(),
    m_firstRunningValue(true),
    m_runningValue(0)
{
    if (m_operation == TorcLogicControl::UnknownLogicType)
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Unrecognised control operation '%1' for device '%2'").arg(Type, uniqueId));
        return;
    }

    // these operations require a valid value to operate against
    if (IsComplexType(m_operation))
    {
        bool found = false;
        if (Details.contains(QStringLiteral("references")))
        {
            QVariantMap reference = Details.value(QStringLiteral("references")).toMap();
            if (reference.contains(QStringLiteral("device")))
            {
                m_referenceDeviceId = reference.value(QStringLiteral("device")).toString().trimmed();
                // and treat it as a normal input - this ensures TorcControl takes care of all of the input value and
                // valid logic. We just ensure we know which is the reference device/value.
                m_inputList.append(m_referenceDeviceId);
                found = true;
            }
        }
        if (!found)
        {
            LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Control '%1' has no reference device for operation").arg(uniqueId));
            return;
        }

        if (m_operation == TorcLogicControl::RunningAverage)
        {
            // trigger device is used to update the average
            found = false;
            if (Details.contains(QStringLiteral("triggers")))
            {
                QVariantMap triggers = Details.value(QStringLiteral("triggers")).toMap();
                if (triggers.contains(QStringLiteral("device")))
                {
                    m_triggerDeviceId = triggers.value(QStringLiteral("device")).toString().trimmed();
                    m_inputList.append(m_triggerDeviceId);
                    found = true;
                }
            }

            if (!found)
            {
                LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Control '%1' has no trigger device for updating").arg(uniqueId));
                return;
            }
        }
        else if (m_operation == TorcLogicControl::RunningMin)
        {
            m_runningValue = std::numeric_limits<double>::max();
        }
        else if (m_operation == TorcLogicControl::RunningMax)
        {
            m_runningValue = std::numeric_limits<double>::min();
        }
    }

    // everything appears to be valid at this stage
    m_inputList.removeDuplicates();
    m_parsed = true;
}

TorcControl::Type TorcLogicControl::GetType(void) const
{
    return TorcControl::Logic;
}

QStringList TorcLogicControl::GetDescription(void)
{
    QStringList result;
    QString reference(QStringLiteral("Unknown"));
    TorcDevice *device = qobject_cast<TorcDevice*>(m_referenceDevice);
    if (device)
    {
        reference = device->GetUserName();
        if (reference.isEmpty())
            reference = device->GetUniqueId();
    }

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
        case TorcLogicControl::RunningAverage:
            result.append(tr("Running average"));
            break;
        case TorcLogicControl::RunningMax:
            result.append(tr("Running max"));
            break;
        case TorcLogicControl::RunningMin:
            result.append(tr("Running min"));
            break;
        case TorcLogicControl::UnknownLogicType:
            result.append(tr("Unknown"));
            break;
    }

    return result;
}

bool TorcLogicControl::IsPassthrough(void)
{
    QMutexLocker locker(&lock);

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
    QMutexLocker locker(&lock);

    // don't repeat validation
    if (m_validated)
        return true;

    // common checks
    if (!TorcControl::Validate())
        return false;

    // we always need one or more outputs
    if (m_outputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QStringLiteral("Device '%1' needs at least one output").arg(uniqueId));
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
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("%1 has %2 inputs for operation '%3' (needs at least 2) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(',')));
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
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("%1 has %2 inputs for operation '%3' (must have 1) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(',')));
                    return false;
                }
            }
            break;
        case TorcLogicControl::Equal:
        case TorcLogicControl::LessThan:
        case TorcLogicControl::LessThanOrEqual:
        case TorcLogicControl::GreaterThan:
        case TorcLogicControl::GreaterThanOrEqual:
        case TorcLogicControl::RunningMax:
        case TorcLogicControl::RunningMin:
            // these should have one defined input and one reference device that is handled as an input
            {
                if (m_inputs.size() != 2)
                {
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("%1 has %2 inputs for operation '%3' (must have 1 input and 1 reference) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(',')));
                    return false;
                }
            }
            break;
        case TorcLogicControl::RunningAverage:
            // one input, one reference (reset) and one trigger (update)
            {
                if (m_inputs.size() != 3)
                {
                    LOG(VB_GENERAL, LOG_ERR, QStringLiteral("%1 has %2 inputs for operation '%3' (must have 1 input, 1 reference and 1 trigger) - ignoring.")
                        .arg(uniqueId).arg(m_inputs.size()).arg(GetDescription().join(',')));
                    return false;
                }
            }
            break;
        case TorcLogicControl::UnknownLogicType:
            break; // should be unreachable
    }

    // 'reference' devices must be valid if TorcControl::Validate is happy
    if (IsComplexType(m_operation))
    {
        m_referenceDevice = gDeviceList->value(m_referenceDeviceId);
        if (!m_referenceDevice)
            return false;
        if (TorcLogicControl::RunningAverage == m_operation)
        {
            m_triggerDevice   = gDeviceList->value(m_triggerDeviceId);
            if (!m_triggerDevice)
                return false;
            QList<QObject*> inputs = m_inputs.uniqueKeys();
            foreach(QObject *device, inputs)
            {
                if (device != m_referenceDevice && device != m_triggerDevice)
                {
                    m_inputDevice = device;
                    break;
                }
            }
        }
        else
        {
            m_inputDevice = m_referenceDevice == m_inputs.firstKey() ? m_inputs.lastKey() : m_inputs.firstKey();
        }
        if (!m_inputDevice)
            return false;
    }

    // if we get this far, we can finish the device
    if (!Finish())
        return false;

    return true;
}

void TorcLogicControl::CalculateOutput(void)
{
    QMutexLocker locker(&lock);

    double newvalue = value; // no change by default

    double referencevalue = 0.0;
    double inputvalue     = 0.0;
    double triggervalue   = 0.0;

    if ((TorcLogicControl::RunningAverage == m_operation) && m_triggerDevice)
        triggervalue = m_inputValues.value(m_triggerDevice);

    if (IsComplexType(m_operation) && m_inputDevice && m_referenceDevice)
    {
        inputvalue = m_inputValues.value(m_inputDevice);
        referencevalue = m_inputValues.value(m_referenceDevice);
    }

    switch (m_operation)
    {
        case TorcLogicControl::RunningAverage:
            // We do not update for a change in value - only when triggered. Reference resets.
            // NB trigger and reset can both be high at the same time - which should probably be avoided
            if (referencevalue) // reset
            {
                m_average.Reset();
                newvalue = m_average.GetAverage();
            }
            // trigger
            if (triggervalue)
            {
                newvalue = m_average.AddValue(inputvalue);
            }
            break;
        case TorcLogicControl::RunningMax:
            // RunningMax/Min are updated each time the input value changes, reset when triggered via reference
            // and always set on first pass.
            // We don't reset to max/min double as inputvalue will provide a valid start point.
            if (referencevalue || m_firstRunningValue || (inputvalue > m_runningValue))
            {
                m_runningValue = newvalue = inputvalue;
                m_firstRunningValue = false;
            }
            break;
        case TorcLogicControl::RunningMin:
            if (referencevalue || m_firstRunningValue || (inputvalue < m_runningValue))
            {
                m_runningValue = newvalue = inputvalue;
                m_firstRunningValue = false;
            }
            break;
        case TorcLogicControl::Passthrough:
            newvalue = m_inputValues.constBegin().value();
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
                if (m_lastInputValues.constBegin().value() < 1.0 && m_inputValues.constBegin().value() >= 1.0)
                    newvalue = value >= 1.0 ? 0.0 : 1.0;
            }
            break;
        case TorcLogicControl::Invert:
            {
                newvalue = m_inputValues.constBegin().value() < 1.0 ? 1.0 : 0.0;
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
        case TorcLogicControl::UnknownLogicType:
            break;
    }
    SetValue(newvalue);
}
