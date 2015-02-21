/* Class TorcControl
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
#include <QMetaMethod>

// Torc
#include "torclogging.h"
#include "torccentral.h"
#include "torcsensor.h"
#include "torcoutput.h"
#include "torccontrol.h"

TorcControl::Operation TorcControl::StringToOperation(const QString &Operation)
{
    QString operation = Operation.toUpper();

    if ("EQUAL" == operation)              return TorcControl::Equal;
    if ("LESSTHAN" == operation)           return TorcControl::LessThan;
    if ("LESSTHANOREQUAL" == operation)    return TorcControl::LessThanOrEqual;
    if ("GREATERTHAN" == operation)        return TorcControl::GreaterThan;
    if ("GREATERTHANOREQUAL" == operation) return TorcControl::GreaterThanOrEqual;
    if ("ANY" == operation)                return TorcControl::Any;
    if ("ALL" == operation)                return TorcControl::All;

    return TorcControl::None;
}

QString TorcControl::OperationToString(TorcControl::Operation Operation)
{
    switch (Operation)
    {
        case TorcControl::Equal:              return QString("Equal");
        case TorcControl::LessThan:           return QString("LessThan");
        case TorcControl::LessThanOrEqual:    return QString("LessThanOrEqual");
        case TorcControl::GreaterThan:        return QString("GreaterThan");
        case TorcControl::GreaterThanOrEqual: return QString("GreaterThanOrEqual");
        case TorcControl::Any:                return QString("Any");
        case TorcControl::All:                return QString("All");
        case TorcControl::None:
        default: break;
    }

    return QString("None");
}

/*! \class TorcControl
 *
 * The control is 'valid' if all of its inputs are present, valid and have a known value.
 * It can then determine an output value.
 * If 'invalid' the output will be set to the default.
*/
TorcControl::TorcControl(const QString &UniqueId, const QStringList &Inputs, const QStringList &Outputs,
                         TorcControl::Operation Operation, double OperationValue)
  : TorcDevice(false, 0, 0, QString("Control"), UniqueId),
    m_validated(false),
    m_inputList(Inputs),
    m_outputList(Outputs),
    m_operation(Operation),
    m_operationValue(OperationValue),
    m_allInputsValid(false)
{
}

TorcControl::~TorcControl()
{
}

/*! \fn Validate
 *
 * If the control has one single sensor input (NOT another control), one or more outputs (NOT controls) and
 * the operation is a straight pass through (Operation::None), then we do not present the control in the stategraph.
 * This makes the graph clearer but the control is still required as Outputs have no understanding of invalid
 * sensor outputs.
 *
 * \note We always assume an object of a given type has the correct signals/slots
 *
 * \todo Validate input/output data types
 * \todo Make Outputs 'valid' aware and handle their own defaults if any input/control is invalid.
*/
void TorcControl::Validate(void)
{
    QMutexLocker locker(lock);

    if (m_validated)
        return;

    bool passthrough = (m_operation == TorcControl::None) && (m_inputList.size() == 1);

    // we need one or more inputs for a regular control
    if (m_inputList.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 needs at least one input").arg(uniqueId));
        return;
    }

    // validate inputs
    foreach (QString input, m_inputList)
    {
        QObject *object = TorcDevice::GetObjectforId(input);

        // valid object
        if (!object)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find input '%1' for '%2'").arg(input).arg(uniqueId));
            return;
        }

        m_inputs.insert(object, input);

        // check for passthrough
        if (!qobject_cast<TorcSensor*>(object))
            passthrough = false;
    }

    // we always need one or more outputs
    if (m_outputList.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("%1 needs at least one output").arg(uniqueId));
        return;
    }

    // validate outputs
    foreach (QString output, m_outputList)
    {
        QObject *object = TorcDevice::GetObjectforId(output);

        // valid object?
        if (!object)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find output '%1' for device %2").arg(output).arg(uniqueId));
            return;
        }

        m_outputs.insert(object, output);

        // check for passthrough
        if (!qobject_cast<TorcOutput*>(object))
            passthrough = false;
    }

    // sanity check number of inputs for operation type
    if (m_operation == TorcControl::Equal ||
        m_operation == TorcControl::LessThan ||
        m_operation == TorcControl::LessThanOrEqual ||
        m_operation == TorcControl::GreaterThan ||
        m_operation == TorcControl::GreaterThanOrEqual)
    {
        // can have only one input to compare against
        if (m_inputs.size() > 1)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (can have only 1) - ignoring.")
                .arg(uniqueId).arg(m_inputs.size()).arg(OperationToString(m_operation)));
            return;
        }
    }
    else if (m_operation == TorcControl::Any ||
             m_operation == TorcControl::All)
    {
        // ANY and ALL imply multiple inputs. Technically they will operate fine with just one
        // but fail and warn - the user needs to know as their config may not be correct
        if (m_inputs.size() < 2)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("%1 has %2 inputs for operation '%3' (needs at least 2) - ignoring.")
                .arg(uniqueId).arg(m_inputs.size()).arg(OperationToString(m_operation)));
            return;
        }
    }

    // if we get this far, everything is validated and we can 'join the dots'
    {
        QMutexLocker locker(TorcCentral::gStateGraphLock);

        if (!passthrough)
            TorcCentral::gStateGraph->append(QString("    \"%1\" [label=\"%2\"];\r\n").arg(uniqueId).arg(userName));

        QMap<QObject*,QString>::iterator it = m_outputs.begin();
        for ( ; it != m_outputs.end(); ++it)
        {
            if (qobject_cast<TorcOutput*>(it.key()))
            {
                TorcOutput* output = qobject_cast<TorcOutput*>(it.key());
                QString source = passthrough ? qobject_cast<TorcSensor*>(m_inputs.firstKey())->GetUniqueId() : uniqueId;
                TorcCentral::gStateGraph->append(QString("    \"%1\"->\"%2\"\r\n").arg(source).arg(output->GetUniqueId()));
                connect(this, SIGNAL(ValueChanged(double)), output, SLOT(SetValue(double)), Qt::UniqueConnection);
                output->DisableMethod("SetValue");
            }
            else
            {
                TorcControl* control = qobject_cast<TorcControl*>(it.key());
                TorcCentral::gStateGraph->append(QString("    \"%1\"->\"%2\"\r\n").arg(uniqueId).arg(control->GetUniqueId()));
                connect(this, SIGNAL(ValidChanged(bool)), control, SLOT(InputValidChanged(bool)), Qt::UniqueConnection);
                connect(this, SIGNAL(ValueChanged(double)), control, SLOT(InputValueChanged(double)), Qt::UniqueConnection);
            }
        }

        it = m_inputs.begin();
        for ( ; it != m_inputs.end(); ++it)
        {
            QString inputid;
            if (qobject_cast<TorcControl*>(it.key()))
                inputid = qobject_cast<TorcControl*>(it.key())->GetUniqueId();
            else if (qobject_cast<TorcSensor*>(it.key()))
                inputid = qobject_cast<TorcSensor*>(it.key())->GetUniqueId();

            if (!passthrough) // already handled in the outputs above s
                TorcCentral::gStateGraph->append(QString("    \"%1\"->\"%2\"\r\n").arg(inputid).arg(uniqueId));
            connect(it.key(), SIGNAL(ValidChanged(bool)), this, SLOT(InputValidChanged(bool)), Qt::UniqueConnection);
            connect(it.key(), SIGNAL(ValueChanged(double)), this, SLOT(InputValueChanged(double)), Qt::UniqueConnection);
            m_inputValids.insert(it.key(), false);
        }
    }

    LOG(VB_GENERAL, LOG_INFO, QString("%1: Ready").arg(uniqueId));
    m_validated = true;
}

void TorcControl::InputValueChanged(double Value)
{
    QMutexLocker locker(lock);

    QObject *input = sender();

    // a valid input
    if (!input)
        return;

    // a known input
    if (!m_inputs.contains(input))
        return;

    // ignore known values
    if (m_inputValues.contains(input) && qFuzzyCompare(m_inputValues.value(input) + 1.0, Value + 1.0))
        return;

    m_inputValues[input] = Value;
    // as for sensors, setting an input value is assumed to make the input valid
    InputValidChangedPriv(input, true);

    // check for an update to the output
    CheckInputValues();
}

void TorcControl::CheckInputValues(void)
{
    QMutexLocker locker(lock);

    bool isvalid = m_validated && m_allInputsValid && (m_inputList.size() == m_inputValues.size());

    SetValid(isvalid);
    if (!isvalid)
        return;

    // the important bit
    double newvalue = value; // no change by default
    switch (m_operation)
    {
        case TorcControl::None:
            if (m_inputs.size() == 1)
            {
                // must be a single input just passed through to output(s)
                newvalue = m_inputValues.first();
            }
            else
            {
                // must be multiple range/pwm values that are combined
                double start = 1.0;
                foreach(double next, m_inputValues)
                    start *= next;
                newvalue = start;
            }
            break;
        case TorcControl::Equal:
            // single value ==
            newvalue = qFuzzyCompare(m_inputValues.first() + 1.0, m_operationValue + 1.0) ? 1 : 0;
            break;
        case TorcControl::LessThan:
            // single value <
            newvalue = m_inputValues.first() < m_operationValue ? 1 : 0;
            break;
        case TorcControl::LessThanOrEqual:
            // Use qFuzzyCompare for = ?
            // single input <=
            newvalue = m_inputValues.first() <= m_operationValue ? 1 : 0;
            break;
        case TorcControl::GreaterThan:
            // single input >
            newvalue = m_inputValues.first() > m_operationValue ? 1 : 0;
            break;
        case TorcControl::GreaterThanOrEqual:
            // single input >=
            newvalue = m_inputValues.first() >= m_operationValue ? 1 : 0;
            break;
        case TorcControl::All:
            {
                // multiple binary (on/off) inputs that must all be 1/On/non-zero
                bool on = true;
                foreach (double next, m_inputValues)
                    on &= !qFuzzyCompare(next + 1.0, 1.0);
                newvalue = on ? 1 : 0;
            }
            break;
        case TorcControl::Any:
            {
                // multiple binary (on/off) inputs - one or more of which must be 1/On/non-zero
                bool on = false;
                foreach (double next, m_inputValues)
                    on |= !qFuzzyCompare(next + 1.0, 1.0);
                newvalue = on ? 1 : 0;
            }
            break;
    }
    SetValue(newvalue);
}

void TorcControl::InputValidChanged(bool Valid)
{
    QMutexLocker locker(lock);

    QObject* input = sender();
    if (input)
    {
        InputValidChangedPriv(input, Valid);
        CheckInputValues();
    }
}

void TorcControl::InputValidChangedPriv(QObject *Input, bool Valid)
{
    // a valid input
    if (!Input)
        return;

    // a known input
    if (!m_inputs.contains(Input))
        return;

    // ignore known values
    if (m_inputValids.contains(Input) && m_inputValids.value(Input) == Valid)
        return;

    m_inputValids[Input] = Valid;
    m_allInputsValid = Valid;

    if (!Valid)
    {
        m_inputValues.remove(Input);
    }
    else
    {
        foreach(bool inputvalid, m_inputValids)
            m_allInputsValid &= inputvalid;
    }
}

bool TorcControl::GetValid(void)
{
    QMutexLocker locker(lock);
    return valid;
}

double TorcControl::GetValue(void)
{
    QMutexLocker locker(lock);
    return value;
}

QString TorcControl::GetUniqueId(void)
{
    QMutexLocker locker(lock);
    return uniqueId;
}

QString TorcControl::GetUserName(void)
{
    QMutexLocker locker(lock);
    return userName;
}

QString TorcControl::GetUserDescription(void)
{
    QMutexLocker locker(lock);
    return userDescription;
}

void TorcControl::SetUserName(const QString &Name)
{
    QMutexLocker locker(lock);

    if (Name == userName)
        return;

    userName = Name;
    emit UserNameChanged(userName);
}

void TorcControl::SetUserDescription(const QString &Description)
{
    QMutexLocker locker(lock);

    if (Description == userDescription)
        return;

    userDescription = Description;
    emit UserDescriptionChanged(userDescription);
}

void TorcControl::SetValue(double Value)
{
    QMutexLocker locker(lock);

    if (qFuzzyCompare(Value + 1.0, value + 1.0))
        return;

    value = Value;
    emit ValueChanged(value);
}

void TorcControl::SetValid(bool Valid)
{
    QMutexLocker locker(lock);

    if (valid == Valid)
        return;

    valid = Valid;
    emit ValidChanged(valid);

    // important!!
    if (!valid)
        SetValue(defaultValue);
}
