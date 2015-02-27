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

TorcControl::Type TorcControl::StringToType(const QString &Type)
{
    QString type = Type.trimmed().toUpper();

    if (type == "LOGIC")      return TorcControl::Logic;
    if (type == "TIMER")      return TorcControl::Timer;
    if (type == "TRANSITION") return TorcControl::Transition;

    return TorcControl::UnknownType;
}

QString TorcControl::TypeToString(TorcControl::Type Type)
{
    switch (Type)
    {
        case TorcControl::Logic:      return QString("logic");
        case TorcControl::Timer:      return QString("timer");
        case TorcControl::Transition: return QString("transition");
        default: break;
    }

    return QString("Unknown");
}

/*! \brief Parse a Torc time string into days, hours, minutes and, if present, seconds.
 *
 * Valid times are of the format MM, HH:MM, DD:HH:MM with an optional trailing .SS for seconds.
*/
bool TorcControl::ParseTimeString(const QString &Time, int &Days, int &Hours,
                                  int &Minutes, int &Seconds, quint64 &DurationInSeconds)
{
    bool ok      = false;
    int  seconds = 0;
    int  hours   = 0;
    int  days    = 0;

    // take seconds off the end if present
    QStringList initialsplit = Time.split(".");

    QString dayshoursminutes = initialsplit[0];
    QString secondss = initialsplit.size() > 1 ? initialsplit[1] : QString("");

    // parse seconds (seconds are optional)
    if (!secondss.isEmpty())
    {
        int newseconds = secondss.toInt(&ok);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse seconds from '%1'").arg(secondss));
            return false;
        }

        if (newseconds < 0 || newseconds > 59)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid seconds value '%1'").arg(newseconds));
            return false;
        }

        seconds = newseconds;
    }

    // parse the remainder
    QStringList secondsplit = dayshoursminutes.split(":");
    int count = secondsplit.size();
    if (count > 3)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Cannot parse time from '%1'").arg(dayshoursminutes));
        return false;
    }

    // days - days are from 1 to 7 (Monday to Sunday), consistent with Qt
    if (count == 3)
    {
        int newdays = secondsplit[0].toInt(&ok);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse days from '%1'").arg(secondsplit[2]));
            return false;
        }

        if (newdays < 1 || newdays > 7)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid day value '%1'").arg(newdays));
            return false;
        }

        days = newdays;
    }

    // hours 0-23
    if (count > 1)
    {
        int newhours = secondsplit[count - 2].toInt(&ok);
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse hours from '%1'").arg(secondsplit[1]));
            return false;
        }

        if (newhours < 0 || newhours > 23)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Invalid hour value '%1'").arg(newhours));
            return false;
        }

        hours = newhours;
    }

    // minutes 0-59
    int newminutes = secondsplit[count - 1].toInt(&ok);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse minutes from '%1'").arg(secondsplit[0]));
        return false;
    }

    if (newminutes < 0 || newminutes > 59)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Invalid minute values '%1'").arg(newminutes));
        return false;
    }

    // all is good in the world
    Days    = days;
    Hours   = hours;
    Minutes = newminutes;
    Seconds = seconds;

    // duration
    DurationInSeconds = seconds + (newminutes * 60) + (hours * 60 * 60) + (days * 24 * 60 * 60);

    return true;
}

/*! \class TorcControl
 *
 * The control is 'valid' if all of its inputs are present, valid and have a known value.
 * It can then determine an output value.
 * If 'invalid' the output will be set to the default.
*/
TorcControl::TorcControl(const QString &UniqueId, const QVariantMap &Details)
  : TorcDevice(false, 0, 0, QString("Control"), UniqueId),
    m_parsed(false),
    m_validated(false),
    m_inputList(),
    m_outputList(),
    m_allInputsValid(false)
{
    // parse inputs
    m_inputList = Details.value("inputs").toStringList();
    m_inputList.removeDuplicates();
    m_inputList.removeAll("");

    // parse outputs
    m_outputList = Details.value("outputs").toStringList();
    m_outputList.removeDuplicates();
    m_outputList.removeAll("");

    // parse other common details
    SetUserName(Details.value("userName").toString());
    SetUserDescription(Details.value("userDescription").toString());
}

TorcControl::~TorcControl()
{
}

/*! \fn Validate
*/
bool TorcControl::Validate(void)
{
    QMutexLocker locker(lock);

    if (!m_parsed)
        return false;

    // validate inputs
    foreach (QString input, m_inputList)
    {
        QObject *object = TorcDevice::GetObjectforId(input);

        // valid object
        if (!object)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find input '%1' for '%2'").arg(input).arg(uniqueId));
            return false;
        }

        m_inputs.insert(object, input);
    }

    // validate outputs
    foreach (QString output, m_outputList)
    {
        QObject *object = TorcDevice::GetObjectforId(output);

        // valid object?
        if (!object)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find output '%1' for device %2").arg(output).arg(uniqueId));
            return false;
        }

        // if TorcOutput, does it already have an owner
        TorcOutput* out = qobject_cast<TorcOutput*>(object);
        if (out && out->HasOwner())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Output '%1' (for control '%2') already has an owner").arg(out->GetUniqueId()));
            return false;
        }

        m_outputs.insert(object, output);
    }

    return true;
}

/// The default start implementation does nothing (for logic control).
void TorcControl::Start(void)
{
}

/*! \fn Finish
 * \brief Finish setup of the control
 *
 * Finish is only called once all other parsing and validation is complete.
 * The control is connected to its input(s) and output(s), the state graph is completed
 * and the device marked as validated.
 *
 * \note If a Logic control has one single sensor input (NOT another control), one or more outputs (NOT controls) and
 *       the operation is a straight pass through (TorcLogicControl::NoOperation), then we do not present the control in the stategraph.
 *       This makes the graph clearer but the control is still required as Outputs have no understanding of invalid
 *       sensor outputs.
 * \note We always assume an object of a given type has the correct signals/slots
 *
 * \todo Validate input/output data types
 * \todo Make Outputs 'valid' aware and handle their own defaults if any input/control is invalid.
*/
void TorcControl::Finish(bool Passthrough)
{
    QMutexLocker locker(TorcCentral::gStateGraphLock);

    if (!Passthrough)
        TorcCentral::gStateGraph->append(QString("    \"%1\" [label=\"%2\"];\r\n").arg(uniqueId).arg(userName));

    QMap<QObject*,QString>::iterator it = m_outputs.begin();
    for ( ; it != m_outputs.end(); ++it)
    {
        if (qobject_cast<TorcOutput*>(it.key()))
        {
            TorcOutput* output = qobject_cast<TorcOutput*>(it.key());
            (void)output->SetOwner(this);
            QString source = Passthrough ? qobject_cast<TorcSensor*>(m_inputs.firstKey())->GetUniqueId() : uniqueId;
            TorcCentral::gStateGraph->append(QString("    \"%1\"->\"%2\"\r\n").arg(source).arg(output->GetUniqueId()));
            connect(this, SIGNAL(ValueChanged(double)), output, SLOT(SetValue(double)), Qt::UniqueConnection);
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

        if (!Passthrough) // already handled in the outputs above
            TorcCentral::gStateGraph->append(QString("    \"%1\"->\"%2\"\r\n").arg(inputid).arg(uniqueId));
        connect(it.key(), SIGNAL(ValidChanged(bool)), this, SLOT(InputValidChanged(bool)), Qt::UniqueConnection);
        connect(it.key(), SIGNAL(ValueChanged(double)), this, SLOT(InputValueChanged(double)), Qt::UniqueConnection);
        m_inputValids.insert(it.key(), false);
    }

    LOG(VB_GENERAL, LOG_INFO, QString("%1: Ready").arg(uniqueId));
    m_validated = true;
}

void TorcControl::InputValueChanged(double Value)
{
    QMutexLocker locker(lock);

    if (!m_parsed || !m_validated)
        return;

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

    if (!m_parsed || !m_validated)
        return;

    bool isvalid = m_validated && m_allInputsValid && (m_inputList.size() == m_inputValues.size());

    SetValid(isvalid);
    if (!isvalid)
        return;

    // the important bit
    CalculateOutput();
}

void TorcControl::InputValidChanged(bool Valid)
{
    QMutexLocker locker(lock);

    if (!m_parsed || !m_validated)
        return;

    QObject* input = sender();
    if (input)
    {
        InputValidChangedPriv(input, Valid);
        CheckInputValues();
    }
}

void TorcControl::InputValidChangedPriv(QObject *Input, bool Valid)
{
    if (!m_parsed || !m_validated)
        return;

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

    if (!m_parsed || !m_validated)
        return;

    if (qFuzzyCompare(Value + 1.0, value + 1.0))
        return;

    value = Value;
    emit ValueChanged(value);
    LOG(VB_GENERAL, LOG_INFO, QString("%1: new value '%2'").arg(uniqueId).arg(value));
}

void TorcControl::SetValid(bool Valid)
{
    QMutexLocker locker(lock);

    if (!m_parsed || !m_validated)
        return;

    if (valid == Valid)
        return;

    valid = Valid;
    emit ValidChanged(valid);

    // important!!
    if (!valid)
        SetValue(defaultValue);
}
