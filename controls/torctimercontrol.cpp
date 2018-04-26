/* Class TorcTimerControl
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
#include <QLocale>
#include <QVariant>

// Torc
#include "torclogging.h"
#include "torclocalcontext.h"
#include "torctimercontrol.h"

static const quint64 kSixty = 60;
static const quint64 k24    = 24;
static const quint64 k7     = 7;

QString TorcTimerControl::TimerTypeToString(TorcTimerControl::TimerType Type)
{
    switch (Type)
    {
        case TorcTimerControl::Custom:   return QString("Custom");
        case TorcTimerControl::Minutely: return QString("Minutely");
        case TorcTimerControl::Hourly:   return QString("Hourly");
        case TorcTimerControl::Daily:    return QString("Daily");
        case TorcTimerControl::Weekly:   return QString("Weekly");
        default: break;
    }

    return QString("Unknown");
}

TorcTimerControl::TimerType TorcTimerControl::StringToTimerType(const QString &Type)
{
    QString type = Type.trimmed().toUpper();

    if ("CUSTOM"   == type) return TorcTimerControl::Custom;
    if ("MINUTELY" == type) return TorcTimerControl::Minutely;
    if ("HOURLY"   == type) return TorcTimerControl::Hourly;
    if ("DAILY"    == type) return TorcTimerControl::Daily;
    if ("WEEKLY"   == type) return TorcTimerControl::Weekly;

    return TorcTimerControl::UnknownTimerType;
}

TorcTimerControl::TorcTimerControl(const QString &Type, const QVariantMap &Details)
  : TorcControl(TorcControl::Timer, Details),
    m_timerType(StringToTimerType(Type)),
    m_startTime(QTime()),
    m_startDay(0),
    m_startDuration(0),
    m_duration(0),
    m_durationTime(QTime()),
    m_durationDay(0),
    m_timer(new QTimer()),
    m_firstTrigger(true)
{
    if (m_timerType == TorcTimerControl::UnknownTimerType)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown timer type '%1' for device '%2'").arg(Details.value("type").toString()).arg(uniqueId));
        return;
    }

    // check for start time
    if (!Details.contains("start"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Timer '%1' does not specify start time").arg(uniqueId));
        return;
    }

    // start time must be in the format DD:HH:MM or HH:MM or MM with an optional .SS appended
    QString start  = Details.value("start").toString();
    int days, hours, minutes, seconds = 0;
    quint64 startduration;

    if (!TorcControl::ParseTimeString(start, days, hours, minutes, seconds, startduration))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse start time from '%1'").arg(start));
        return;
    }

    m_startTime         = QTime(hours, minutes, seconds);
    m_startDay          = days;
    m_startDuration = startduration;

    // check duration - format as for start time
    if (!Details.contains("duration"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Timer '%1' does not specify duration").arg(uniqueId));
        return;
    }

    QString durations = Details.value("duration").toString();
    quint64 duration;
    if (!TorcControl::ParseTimeString(durations, days, hours, minutes, seconds, duration))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse duration from '%1'").arg(durations));
        return;
    }

    m_duration          = duration;
    m_durationTime = QTime(hours, minutes, seconds);
    m_durationDay       = days;

    // sanity check start time and duration in relation to timer type
    if (m_timerType == TorcTimerControl::Custom)
    {
        // the timer will trigger every 'startduration' seconds and last for 'duration'
        // need a minimum of 1 second frequency and 1 second duration - otherwise pointless
        if (startduration < 1 || duration < 1)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Custom timer frequency/duration is too low (%1)").arg(startduration));
            return;
        }
    }
    else
    {
        quint64 maxduration = GetMaxDuration();
        if ((startduration >= maxduration) || ((startduration + duration) > maxduration))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Start and/or duration invalid for time type '%1' (start '%2' duration '%3')")
                .arg(TimerTypeToString(m_timerType)).arg(start).arg(durations));
            return;
        }
    }

    // everything appears to be valid at this stage
    m_parsed = true;
}

TorcTimerControl::~TorcTimerControl()
{
    delete m_timer;
}

TorcControl::Type TorcTimerControl::GetType(void) const
{
    return TorcControl::Timer;
}

QStringList TorcTimerControl::GetDescription(void)
{
    QStringList result;

    if (TorcTimerControl::Custom == m_timerType)
    {
        QString frequency = m_startDay > 0 ? QString("%1days %2").arg(m_startDay).arg(m_startTime.toString(QString("hh:mm.ss"))) :
                                             QString("%2").arg(m_startTime.toString(QString("hh:mm.ss")));
        QString length   = TorcControl::DurationToString(m_durationDay, m_duration);

        result.append(tr("Custom Timer"));
        result.append(tr("Frequency %1").arg(frequency));
        result.append(tr("Duration %2").arg(length));
    }
    else if (TorcTimerControl::Minutely == m_timerType)
    {
        result.append(tr("Minutely Timer"));
        result.append(tr("Start %1").arg(m_startTime.toString(QString("ss"))));
        result.append(tr("Duration %2").arg(QTime(0, 0).addSecs(m_duration).toString(QString("ss"))));
    }
    else if (TorcTimerControl::Hourly == m_timerType)
    {
        result.append(tr("Hourly Timer"));
        result.append(tr("Start %1").arg(m_startTime.toString(QString("mm.ss"))));
        result.append(tr("Duration %2").arg(QTime(0, 0).addSecs(m_duration).toString(QString("mm.ss"))));
    }
    else if (TorcTimerControl::Daily == m_timerType)
    {
        result.append(tr("Daily Timer"));
        result.append(tr("Start %1").arg(m_startTime.toString(QString("hh:mm.ss"))));
        result.append(tr("Duration %2").arg(QTime(0, 0).addSecs(m_duration).toString(QString("hh:mm.ss"))));
    }
    else if (TorcTimerControl::Weekly == m_timerType)
    {
        result.append(tr("Weekly Timer"));
        result.append(tr("Start %1 %2")
                      .arg(gLocalContext->GetLocale().dayName(m_startDay))
                      .arg(m_startTime.toString(QString("hh:mm.ss"))));
        result.append(tr("Duration %3days %4")
                      .arg(m_durationDay)
                      .arg(QTime(0, 0).addSecs(m_duration).toString(QString("hh:mm.ss"))));
    }
    else
    {
        result.append(tr("Unknown"));
    }

    return result;
}

bool TorcTimerControl::Validate(void)
{
    QMutexLocker locker(lock);

    // don't repeat validation
    if (m_validated)
        return true;

    // common checks
    if (!TorcControl::Validate())
        return false;

    // a timer has no inputs
    if (!m_inputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Timer device '%1' cannot have inputs").arg(uniqueId));
        return false;
    }

    // need at least one output
    if (m_outputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Timer device '%1' needs at least one output").arg(uniqueId));
        return false;
    }

    // if we get this far, we can finish the device
    if (!Finish())
        return false;

    // debug
    LOG(VB_GENERAL, LOG_DEBUG, QString("Timer '%1': %2").arg(uniqueId).arg(GetDescription().join(",")));

    // this is probably overkill - but we shouldn't have that many timers and most will
    // have relatively long durations
    m_timer->setTimerType(Qt::PreciseTimer);

    // connect the timer
    connect(m_timer, SIGNAL(timeout()), this, SLOT(TimerTimeout()));

    // all timers are single shot and reset at each trigger
    m_timer->setSingleShot(true);

    return true;
}

/*! \brief Initialise the timer
 *
 * \note All TorcTimerControl internals operate with second accuracy and QTimer
 *       uses millisecond intervals. ALWAYS convert.
*/
void TorcTimerControl::Start(void)
{
    QMutexLocker locker(lock);

    if (!m_parsed || !m_validated)
        return;

    // we want to start a custom timer in the off state. The default state is off and
    // calling TimerTimeout will toggle it to the on state - so fudge to on now.
    // 'Regular' timers will work out the correct state each time TimerTimeout is called.
    if (m_timerType == TorcTimerControl::Custom)
        value = 1;

    // trigger the timer for the next state change
    TimerTimeout();

    // ensure state is communicated
    SetValid(true);
    emit ValueChanged(value);
}

/// Timers cannot have inputs
bool TorcTimerControl::AllowInputs(void) const
{
    return false;
}

void TorcTimerControl::TimerTimeout(void)
{
    QMutexLocker locker(lock);

    bool first = m_firstTrigger;
    m_firstTrigger = false;

    switch (m_timerType)
    {
        case TorcTimerControl::Custom:
            // toggle our value each time the timer times out.
            // N.B. Custom timers are really intended for relatively high frequency (at most a couple
            // of hours) operations (e.g. circulation pumps). Using them for anything else is not recommended,
            // not least because the behaviour is prescriptive at startup.
            {
                m_timer->start((value > 0 ? m_startDuration : m_duration) * 1000);
                SetValue(value > 0 ? 0 : 1);
                return;
            }
            break;
            // for other timers, we need to establish the current time, the current state
            // and when the next trigger is needed
        case TorcTimerControl::Minutely:
        case TorcTimerControl::Hourly:
        case TorcTimerControl::Daily:
        case TorcTimerControl::Weekly:
            {
                QTime timenow = QTime::currentTime();
                QDate datenow = QDate::currentDate();
                int       day = datenow.dayOfWeek() - 1;

                // clip hours for Hourly
                if (TorcTimerControl::Hourly == m_timerType)
                    timenow = QTime(0, timenow.minute(), timenow.second(), timenow.msec());
                // clip hours and minutes for minutely
                else if (TorcTimerControl::Minutely == m_timerType)
                    timenow = QTime(0, 0, timenow.second(), timenow.msec());
                // clip days for non Weekly
                if (TorcTimerControl::Weekly != m_timerType)
                    day = 0;

                quint64 timesinceperiodstart = (day * kSixty * kSixty * k24) +
                                                timenow.second() +
                                                timenow.minute() * kSixty +
                                                timenow.hour()   * kSixty * kSixty;
                quint64 finishtime = m_startDuration + m_duration;

                // hasn't started yet
                if (timesinceperiodstart < m_startDuration)
                {
                    SetValue(0);
                    m_timer->start((m_startDuration - timesinceperiodstart) * 1000);
                    return;
                }
                // should be on now
                else if (timesinceperiodstart >= m_startDuration && timesinceperiodstart < finishtime)
                {
                    // the aim here is to guarantee the 'on' period by having one single timer from start
                    // to end. Start times may shift due to DST changes but we will always aim to maintain
                    // the duration.
                    // If the timers are working correctly (excluding initial startup), then this state should be
                    // reached on or very slightly after the start time - so check. A late timer should only
                    // be fractions of a seconds late at most - and we operate to seconds.
                    quint64 onduration = finishtime - timesinceperiodstart;
                    if (!first && (onduration != m_duration))
                    {
                        LOG(VB_GENERAL, LOG_WARNING, QString("Starting timer 'On' period for %1seconds but it should be %2seconds")
                            .arg(onduration).arg(m_duration));
                    }
                    else if (first)
                    {
                        LOG(VB_GENERAL, LOG_INFO, QString("Starting timer late - will run for %1seconds instead of %2seconds")
                            .arg(onduration).arg(m_duration));
                    }
                    SetValue(1);
                    m_timer->start(onduration * 1000);
                    return;
                }
                // finished
                else if (timesinceperiodstart >= finishtime)
                {
                    // NB always timeout at the top of the hour/day/week. This avoids long timers,
                    // handles drift and better accomodates complications arising from daylight savings changes.
                    SetValue(0);
                    if (m_timerType == TorcTimerControl::Weekly)
                        m_timer->start(((kSixty * kSixty * k24 * k7) - timesinceperiodstart) * 1000);
                    else if (m_timerType == TorcTimerControl::Daily)
                        m_timer->start(((kSixty * kSixty * k24) - timesinceperiodstart) * 1000);
                    else if (m_timerType == TorcTimerControl::Hourly)
                        m_timer->start(((kSixty * kSixty) - timesinceperiodstart) * 1000);
                    else
                        m_timer->start((kSixty - timesinceperiodstart) * 1000);
                    return;
                }
            }
            break;
        default:
            break;
    }

    // should be unreachable
    LOG(VB_GENERAL, LOG_ERR, "Unknown timer state - SERIOUS ERROR");
}

TorcTimerControl::TimerType TorcTimerControl::GetTimerType(void) const
{
    return m_timerType;
}

quint64 TorcTimerControl::TimeSinceLastTransition(void)
{
    QMutexLocker locker(lock);

    // Custom timers don't have a defined start time
    if (TorcTimerControl::Custom == m_timerType ||
        TorcTimerControl::UnknownTimerType == m_timerType)
        return 0;

    // copied from TimerTimeout
    QTime timenow = QTime::currentTime();
    QDate datenow = QDate::currentDate();
    int       day = datenow.dayOfWeek() - 1;

    // clip hours for Hourly
    if (TorcTimerControl::Hourly == m_timerType)
        timenow = QTime(0, timenow.minute(), timenow.second(), timenow.msec());
    // clip hours and minutes for minutely
    else if (TorcTimerControl::Minutely == m_timerType)
        timenow = QTime(0, 0, timenow.second(), timenow.msec());
    // clip days for non Weekly
    if (TorcTimerControl::Weekly != m_timerType)
        day = 0;

    quint64 timesinceperiodstart = (day * kSixty * kSixty * k24) +
                                    timenow.second() +
                                    timenow.minute() * kSixty +
                                    timenow.hour()   * kSixty * kSixty;
    quint64 finishtime = m_startDuration + m_duration;

    if (timesinceperiodstart > finishtime)
    {
        return timesinceperiodstart - finishtime;
    }
    else if (timesinceperiodstart >= m_startDuration && timesinceperiodstart < finishtime)
    {
        return timesinceperiodstart - m_startDuration;
    }

    // time since start of period AND time between end of timer and end of last period
    return timesinceperiodstart + GetMaxDuration() - finishtime;
}

quint64 TorcTimerControl::GetMaxDuration(void) const
{
    switch (m_timerType)
    {
        case TorcTimerControl::Minutely: return kSixty;
        case TorcTimerControl::Hourly:   return kSixty * kSixty;
        case TorcTimerControl::Daily:    return kSixty * kSixty * k24;
        case TorcTimerControl::Weekly:   return kSixty * kSixty * k24 * k7;
        default:
            break;
    }

    return 0;
}

void TorcTimerControl::CalculateOutput(void)
{
}
