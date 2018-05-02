/* Class TorcControl
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
#include <QMetaMethod>

// Torc
#include "torclogging.h"
#include "torccentral.h"
#include "torcinput.h"
#include "torcoutput.h"
#include "../notify/torcnotification.h"
#include "torccontrol.h"

TorcControl::Type TorcControl::StringToType(const QString &Type)
{
    QString type = Type.trimmed().toUpper();

    if (type == "LOGIC")      return TorcControl::Logic;
    if (type == "TIMER")      return TorcControl::Timer;
    if (type == "TRANSITION") return TorcControl::Transition;

    return TorcControl::Unknown;
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

QString TorcControl::DurationToString(int Days, quint64 Duration)
{
    return Days > 0 ? QString("%1days %2").arg(Days).arg(QTime(0, 0).addSecs(Duration).toString(QString("hh:mm.ss"))) :
                      QString("%1").arg(QTime(0, 0).addSecs(Duration).toString(QString("hh:mm.ss")));
}

#define BLACKLIST QString("SetValue,SetValid,InputValueChanged,InputValidChanged")

/*! \class TorcControl
 *
 * The control is 'valid' if all of its inputs are present, valid and have a known value.
 * It can then determine an output value.
 * If 'invalid' the output will be set to the default.
*/
TorcControl::TorcControl(TorcControl::Type Type, const QVariantMap &Details)
  : TorcDevice(false, 0, 0, QString("Control"), Details),
    TorcHTTPService(this, CONTROLS_DIRECTORY + "/" + TypeToString(Type) + "/" + Details.value("name").toString(),
                    Details.value("name").toString(), TorcControl::staticMetaObject, BLACKLIST),
    m_parsed(false),
    m_validated(false),
    m_inputList(),
    m_outputList(),
    m_inputs(),
    m_outputs(),
    m_inputValues(),
    m_lastInputValues(),
    m_inputValids(),
    m_allInputsValid(false)
{
    // parse inputs
    QVariantMap inputs = Details.value("inputs").toMap();
    QVariantMap::const_iterator it = inputs.constBegin();
    for ( ; it != inputs.constEnd(); ++it)
        if (it.key() == "device")
            m_inputList.append(it.value().toString().trimmed());
    m_inputList.removeDuplicates();

    // parse outputs
    QVariantMap outputs = Details.value("outputs").toMap();
    it = outputs.constBegin();
    for (; it != outputs.constEnd(); ++it)
        if (it.key() == "device")
            m_outputList.append(it.value().toString().trimmed());
    m_outputList.removeDuplicates();
}

TorcControl::~TorcControl()
{
}

bool TorcControl::Validate(void)
{
    QMutexLocker locker(&lock);
    // lock the device list as well to ensure device aren't deleted while we're using them
    QMutexLocker locke2(gDeviceListLock);

    if (!m_parsed)
        return false;

    // validate inputs
    foreach (QString input, m_inputList)
    {
        if (input == uniqueId)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' cannot have itself as input").arg(input));
            return false;
        }

        // valid object
        if (!gDeviceList->contains(input))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find input '%1' for '%2'").arg(input).arg(uniqueId));
            return false;
        }

        QObject *object = gDeviceList->value(input);

        // if input is a control device, it MUST expect this device as an output
        TorcControl *control = qobject_cast<TorcControl*>(object);
        if (control && !control->IsKnownOutput(uniqueId))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Device '%1' does not recognise '%2' as an output")
                .arg(input).arg(uniqueId));
            return false;
        }

        m_inputs.insert(object, input);
    }

    // validate outputs
    foreach (QString output, m_outputList)
    {
        if (output == uniqueId)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' cannot have itself as output").arg(output));
            return false;
        }

        // valid object?
        if (!gDeviceList->contains(output))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to find output '%1' for device %2").arg(output).arg(uniqueId));
            return false;
        }

        QObject *object = gDeviceList->value(output);

        // if TorcOutput, does it already have an owner
        TorcOutput* out = qobject_cast<TorcOutput*>(object);
        if (out && out->HasOwner())
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Output '%1' (for control '%2') already has an owner")
                .arg(out->GetUniqueId()).arg(uniqueId));
            return false;
        }

        // if output is a control device, it MUST expect this as an input
        TorcControl* control = qobject_cast<TorcControl*>(object);
        if (control && !control->IsKnownInput(uniqueId))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Device '%1' does not recognise '%2' as an input")
                .arg(output).arg(uniqueId));
            return false;
        }

        m_outputs.insert(object, output);
    }

    return true;
}

/// Only certain logic controls can be passthrough
bool TorcControl::IsPassthrough(void)
{
    return false;
}

/// Most controls have an input side.
bool TorcControl::AllowInputs(void) const
{
    return true;
}

QString TorcControl::GetUIName(void)
{
    QMutexLocker locker(&lock);
    if (userName.isEmpty())
        return uniqueId;
    return userName;
}

bool TorcControl::IsKnownInput(const QString &Input) const
{
    if (Input.isEmpty() || !AllowInputs())
        return false;

    return m_inputList.contains(Input);
}

bool TorcControl::IsKnownOutput(const QString &Output) const
{
    if (Output.isEmpty())
        return false;

    return m_outputList.contains(Output);
}

/*! \brief Add this control to the state graph
*
* \note If a Logic control has one single sensor input (NOT another control), one or more outputs (NOT controls) and
*       the operation is a straight pass through (TorcLogicControl::NoOperation), then we do not present the control in the stategraph.
*       This makes the graph clearer but the control is still required as Outputs have no understanding of invalid
*       sensor outputs.
* */
void TorcControl::Graph(QByteArray* Data)
{
    if (!Data)
        return;

    QMutexLocker locker(&lock);

    bool passthrough = IsPassthrough();

    if (!passthrough)
    {
        QString desc;
        QStringList source = GetDescription();
        foreach (QString item, source)
            desc.append(QString(DEVICE_LINE_ITEM).arg(item));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Default %1").arg(GetDefaultValue())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Valid %1").arg(GetValid())));
        desc.append(QString(DEVICE_LINE_ITEM).arg(tr("Value %1").arg(GetValue())));

        Data->append(QString("    \"%1\" [shape=record id=\"%1\" label=<<B>%2</B>%3>];\r\n")
                             .arg(uniqueId).arg(userName).arg(desc));
    }

    QMap<QObject*,QString>::const_iterator it = m_outputs.constBegin();
    for ( ; it != m_outputs.constEnd(); ++it)
    {
        if (qobject_cast<TorcOutput*>(it.key()))
        {
            TorcOutput* output = qobject_cast<TorcOutput*>(it.key());
            QString source = passthrough ? qobject_cast<TorcInput*>(m_inputs.firstKey())->GetUniqueId() : uniqueId;
            Data->append(QString("    \"%1\"->\"%2\"\r\n").arg(source).arg(output->GetUniqueId()));
        }
        else if (qobject_cast<TorcControl*>(it.key()))
        {
            TorcControl* control = qobject_cast<TorcControl*>(it.key());
            Data->append(QString("    \"%1\"->\"%2\"\r\n").arg(uniqueId).arg(control->GetUniqueId()));
        }
        else if (qobject_cast<TorcNotification*>(it.key()))
        {
            // handled in TorcNotification
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Unknown output type");
        }
    }

    it = m_inputs.constBegin();
    for ( ; it != m_inputs.constEnd(); ++it)
    {
        if (passthrough)
            continue;

        QString inputid;
        if (qobject_cast<TorcControl*>(it.key()))
            inputid = qobject_cast<TorcControl*>(it.key())->GetUniqueId();
        else if (qobject_cast<TorcInput*>(it.key()))
            inputid = qobject_cast<TorcInput*>(it.key())->GetUniqueId();
        else
            LOG(VB_GENERAL, LOG_ERR, "Unknown input type");

        Data->append(QString("    \"%1\"->\"%2\"\r\n").arg(inputid).arg(uniqueId));
    }
}

/*! \brief Finish setup of the control
 *
 * Finish is only called once all other parsing and validation is complete.
 * The control is connected to its input(s) and output(s) and the device marked as validated.
 * Qt::UniqueConnection prevents multiple slot triggering if 'paired' devices identify the same
 * connection.
 *
 * \note An output can have only one input/owner.
 * \note For outputs, we only connect to TorcOutput objects or TorcControl objects that have inputs. So
 *       no connection to TorcTimerControl or any TorcInput.
 * \note For inputs, any sensor or control type is valid.
 * \note We always assume an object of a given type has the correct signals/slots.
 *
 * \todo Make Outputs 'valid' aware and handle their own defaults if any input/control is invalid.
*/
bool TorcControl::Finish(void)
{
    QMutexLocker locker(&lock);

    QMap<QObject*,QString>::const_iterator it = m_outputs.constBegin();
    for ( ; it != m_outputs.constEnd(); ++it)
    {
        // an output must be a type that accepts inputs
        // i.e. a TorcOutput, most TorcControl types and some TorcNotification types
        if (qobject_cast<TorcOutput*>(it.key()))
        {
            TorcOutput* output = qobject_cast<TorcOutput*>(it.key());
            // there can be only one owner... though this is already checked in Validate
            if (!output->SetOwner(this))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Cannot set control '%1' as owner of output '%2'")
                    .arg(uniqueId).arg(output->GetUniqueId()));
                return false;
            }

            connect(this, SIGNAL(ValueChanged(double)), output, SLOT(SetValue(double)), Qt::UniqueConnection);
            connect(this, SIGNAL(ValidChanged(bool)),   output, SLOT(SetValid(bool)),   Qt::UniqueConnection);
        }
        else if (qobject_cast<TorcControl*>(it.key()))
        {
            TorcControl* control = qobject_cast<TorcControl*>(it.key());

            // this will also check whether the control allows inputs
            if (!control->IsKnownInput(uniqueId))
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' does not recognise '%2' as an input")
                    .arg(control->GetUniqueId()).arg(uniqueId));
                return false;
            }

            connect(this, SIGNAL(ValidChanged(bool)), control, SLOT(InputValidChanged(bool)), Qt::UniqueConnection);
            connect(this, SIGNAL(ValueChanged(double)), control, SLOT(InputValueChanged(double)), Qt::UniqueConnection);
        }
        else if (qobject_cast<TorcNotification*>(it.key()))
        {
            // NB TorcNotification connects itself to the control. We only really check this
            // to ensure all control outputs are valid and a control may only exist to trigger
            // a notification.
            TorcNotification* notification = qobject_cast<TorcNotification*>(it.key());
            if (!notification->IsKnownInput(uniqueId))
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Notification '%1' does not recognise '%2' as an input")
                    .arg(notification->GetUniqueId()).arg(uniqueId));
                return false;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Unknown output type for '%1'").arg(uniqueId));
            return false;
        }
    }

    it = m_inputs.constBegin();
    for ( ; it != m_inputs.constEnd(); ++it)
    {
        TorcControl *control = qobject_cast<TorcControl*>(it.key());

        // an input must be a sensor or control
        if (!qobject_cast<TorcInput*>(it.key()) && !control)
        {
            LOG(VB_GENERAL, LOG_ERR, "Unknown input type");
            return false;
        }

        // if input is a control, it must recognise this control as an output
        if (control && !control->IsKnownOutput(uniqueId))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Control '%1' does not recognise control '%2' as an output")
                .arg(control->GetUniqueId()).arg(uniqueId));
            return false;
        }

        connect(it.key(), SIGNAL(ValidChanged(bool)), this, SLOT(InputValidChanged(bool)), Qt::UniqueConnection);
        connect(it.key(), SIGNAL(ValueChanged(double)), this, SLOT(InputValueChanged(double)), Qt::UniqueConnection);
        m_inputValids.insert(it.key(), false);
    }

    LOG(VB_GENERAL, LOG_DEBUG, QString("%1: Ready").arg(uniqueId));
    m_validated = true;
    return true;
}

void TorcControl::SubscriberDeleted(QObject *Subscriber)
{
    TorcHTTPService::HandleSubscriberDeleted(Subscriber);
}

void TorcControl::InputValueChanged(double Value)
{
    QMutexLocker locker(&lock);

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
    if (m_inputValids.contains(input) && m_inputValids.value(input) == true)
        if (m_inputValues.contains(input) && qFuzzyCompare(m_inputValues.value(input) + 1.0, Value + 1.0))
            return;

    // set the last value if known. Otherwise set to new which will not trigger any change.
    double lastvalue = m_inputValues.contains(input) ? m_inputValues.value(input) : Value;

    m_inputValues[input]     = Value;
    m_lastInputValues[input] = lastvalue;

    // as for sensors, setting an input value is assumed to make the input valid
    InputValidChangedPriv(input, true);

    // check for an update to the output
    CheckInputValues();
}

void TorcControl::CheckInputValues(void)
{
    QMutexLocker locker(&lock);

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
    QMutexLocker locker(&lock);

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
    m_allInputsValid     = Valid;

    if (!Valid)
    {
        m_inputValues.remove(Input);
        m_lastInputValues.remove(Input);
    }
    else
    {
        foreach(bool inputvalid, m_inputValids)
            m_allInputsValid &= inputvalid;
    }
}

void TorcControl::SetValue(double Value)
{
    QMutexLocker locker(&lock);

    if (m_parsed && m_validated)
        TorcDevice::SetValue(Value);
}

void TorcControl::SetValid(bool Valid)
{
    QMutexLocker locker(&lock);

    if (m_parsed && m_validated)
    {
        // important!!
        // do this before SetValid as setting a value automatically sets validity.
        if (!Valid)
            SetValue(defaultValue);

        TorcDevice::SetValid(Valid);
    }
}

bool TorcControl::CheckForCircularReferences(const QString &UniqueId, QString Path) const
{
    if (UniqueId.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid UniqueId");
        return false;
    }

    QString path = Path;
    if (!path.isEmpty())
        path += "->";
    path += uniqueId;

    // iterate over the outputs list
    QMap<QObject*,QString>::const_iterator it = m_outputs.constBegin();
    for ( ; it != m_outputs.constEnd(); ++it)
    {
        // check first for a direct match with the output
        if (it.value() == UniqueId)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Circular reference found: %1->%2").arg(path).arg(UniqueId));
            return false;
        }

        // and then ask each output to check
        TorcControl *control = dynamic_cast<TorcControl*>(it.key());
        if (control)
            if (!control->CheckForCircularReferences(UniqueId, path))
                return false;
    }

    return true;
}
