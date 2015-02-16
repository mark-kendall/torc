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
*/
void TorcControl::Validate(void)
{
    QMutexLocker locker(lock);

    if (m_validated)
        return;

    // need one or more outputs
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

        // is it another control or an output?
        TorcOutput  *outputobject  = qobject_cast<TorcOutput*>(object);
        TorcControl *controlobject = qobject_cast<TorcControl*>(object);

        if (outputobject)
        {
            // an output is not designated valid - it is just given a value (it cannot be indeterminate)
            // if this control becomes invlalid, its value will return to the default.
            // has SetValue slot?
            if (outputobject->staticMetaObject.indexOfSlot("SetValue(double)") < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Output '%1' has no SetValue slot").arg(output));
                return;
            }
        }
        else if (controlobject)
        {
            // has valid update slot?
            if (controlobject->staticMetaObject.indexOfSlot("InputValidChanged(bool)") < 0)
            {

                LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' ('%2') has no InputValidChanged slot")
                    .arg(output).arg(controlobject->staticMetaObject.className()));
                return;
            }

            // has value update slot?
            if (controlobject->staticMetaObject.indexOfSlot("InputValueChanged(double)") < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' has no InputValueChanged slot").arg(output));
                return;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Output '%1' has unknown/unsupported type").arg(output));
            return;
        }

        m_outputs.insert(object, output);
    }

    // if we get this far, everything is validated and we can 'join the dots'
    QMap<QObject*,QString>::iterator it = m_outputs.begin();
    for ( ; it != m_outputs.end(); ++it)
    {
        if (qobject_cast<TorcOutput*>(it.key()))
        {
            connect(this, SIGNAL(ValueChanged(double)), qobject_cast<TorcOutput*>(it.key()), SLOT(SetValue(double)));
        }
        else
        {
            connect(this, SIGNAL(ValidChanged(bool)), qobject_cast<TorcControl*>(it.key()), SLOT(InputValidChanged(bool)));
            connect(this, SIGNAL(ValueChanged(double)), qobject_cast<TorcControl*>(it.key()), SLOT(InputValueChanged(double)));
        }
    }

    it = m_inputs.begin();
    for ( ; it != m_inputs.end(); ++it)
    {
        connect(it.key(), SIGNAL(ValidChanged(bool)), this, SLOT(InputValidChanged(bool)));
        connect(it.key(), SIGNAL(ValueChanged(double)), this, SLOT(InputValueChanged(double)));
        m_inputValids.insert(it.key(), false);
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
    if (m_inputValues.contains(input) && qFuzzyCompare(m_inputValues.value(input) + 1.0, 1.0))
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
            newvalue = qFuzzyCompare(m_inputValues.first() + 1.0, m_operationValue) ? 1 : 0;
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

    if (qFuzzyCompare(Value + 1.0, 1.0))
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
