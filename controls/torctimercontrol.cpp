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

static const quint64 kThou  = 1000;
static const quint64 kSixty = 60;
static const quint64 k24    = 24;
static const quint64 k7     = 7;
static const quint64 kMSecsInMinute = kSixty * kThou;
static const quint64 kMSecsInHour   = kMSecsInMinute * kSixty;
static const quint64 kMSecsInDay    = kMSecsInHour * k24;
static const quint64 kMSecsinWeek   = kMSecsInDay * k7;

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
    m_startDay(0),
    m_startTime(0),
    m_duration(0),
    m_durationDay(0),
    m_timer(),
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
    quint64 starttime;
    if (!TorcControl::ParseTimeString(start, days, hours, minutes, seconds, starttime))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse start time from '%1'").arg(start));
        return;
    }

    m_startDay  = days;
    m_startTime = starttime * 1000;

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

    m_duration    = duration * 1000;
    m_durationDay = days;

    // sanity check start time and duration in relation to timer type
    if (m_timerType == TorcTimerControl::Custom)
    {
        // the timer will trigger every 'startduration' seconds and last for 'duration'
        // need a minimum of 1 second frequency and 1 second duration - otherwise pointless
        if (m_startTime < 1000 || m_duration < 1000)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Custom timer frequency/duration is too low (%1:%2)").arg(m_startTime/1000).arg(m_duration/1000));
            return;
        }
    }
    else
    {
        quint64 maxduration = GetPeriodDuration();
        // constraints:
        // - we need a defined off and on period
        // - the on period can be anywhere in the cycle (start, middle, end)
        // - the current on period must finish before the next is due to start
        // - so:
        //   - start < max
        //   - duration > 0
        //   - duration < max
        // - should cover all eventualities...
        if ((m_startTime >= maxduration) || (m_duration < 1000) || (m_duration >= maxduration))
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
}

TorcControl::Type TorcTimerControl::GetType(void) const
{
    return TorcControl::Timer;
}

QStringList TorcTimerControl::GetDescription(void)
{
    QStringList result;

    QString length = TorcControl::DurationToString(m_durationDay, m_duration/1000);
    QTime time(0,0,0,0);
    time = time.addMSecs(m_startTime);
    if (TorcTimerControl::Custom == m_timerType)
    {
        QString frequency = m_startDay > 0 ? QString("%1days %2").arg(m_startDay).arg(time.toString(QString("hh:mm.ss"))) :
                                             QString("%2").arg(time.toString(QString("hh:mm.ss")));
        result.append(tr("Custom Timer"));
        result.append(tr("Frequency %1").arg(frequency));
        result.append(tr("Duration %1").arg(length));
    }
    else if (TorcTimerControl::Minutely == m_timerType)
    {
        result.append(tr("Minutely Timer"));
        result.append(tr("Start %1").arg(time.toString(QString("hh:mm.ss"))));
        result.append(tr("Duration %1").arg(length));
    }
    else if (TorcTimerControl::Hourly == m_timerType)
    {
        result.append(tr("Hourly Timer"));
        result.append(tr("Start %1").arg(time.toString(QString("hh:mm.ss"))));
        result.append(tr("Duration %1").arg(length));
    }
    else if (TorcTimerControl::Daily == m_timerType)
    {
        result.append(tr("Daily Timer"));
        result.append(tr("Start %1").arg(time.toString(QString("hh:mm.ss"))));
        result.append(tr("Duration %1").arg(length));
    }
    else if (TorcTimerControl::Weekly == m_timerType)
    {
        result.append(tr("Weekly Timer"));
        result.append(tr("Start %1 %2")
                      .arg(gLocalContext->GetLocale().dayName(m_startDay))
                      .arg(time.toString(QString("hh:mm.ss"))));
        result.append(tr("Duration %1days %2")
                      .arg(m_durationDay)
                      .arg(length));
    }
    else
    {
        result.append(tr("Unknown"));
    }

    return result;
}

bool TorcTimerControl::Validate(void)
{
    QMutexLocker locker(&lock);

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

    m_timer.setTimerType(Qt::PreciseTimer);

    // connect the timer
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(TimerTimeout()));

    // all timers are single shot and reset at each trigger
    m_timer.setSingleShot(true);

    return true;
}

/*! \brief Initialise the timer
 *
 * \note Timers are configured and presented with second accuracy but we use
 *       milliseconds internally.
*/
void TorcTimerControl::Start(void)
{
    QMutexLocker locker(&lock);

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
    QMutexLocker locker(&lock);

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
                m_timer.start(value > 0 ? m_startTime : m_duration);
                SetValue(value > 0 ? 0 : 1);
                return;
            }
            break;
        case TorcTimerControl::Minutely:
        case TorcTimerControl::Hourly:
        case TorcTimerControl::Daily:
        case TorcTimerControl::Weekly:
            // for other timers, we need to establish the current time, the current state
            // and when the next trigger is needed.
            // A timer that does not cross a period boundary has a pattern of
            // off/on, off/on/off or on/off. '_-' '_-_' '-_'
            // A timer that DOES cross a boundary will have a pattern of
            // off/on/boundary/on/off '_-|-_' for its total cycle but within
            // the period it looks like '-_-' for the two overlapping sequences
            // so we have 4 total combinations to handle...
            {
                quint64 msecsinceperiodstart = MsecsSincePeriodStart();
                quint64 finishtime   = m_startTime + m_duration;
                quint64 periodlength = GetPeriodDuration();
                bool    newvalue     = false;
                quint64 nexttimer    = 0;

                // start is zero
                // on/off -_
                if (m_startTime == 0)
                {
                    newvalue  = msecsinceperiodstart <= m_duration;
                    nexttimer = newvalue ? m_duration - msecsinceperiodstart : periodlength - msecsinceperiodstart;
                }
                // start + duration < max
                // off/on/off _-_
                else if (finishtime < periodlength)
                {
                    if (msecsinceperiodstart < m_startTime)
                    {
                        newvalue  = false;
                        nexttimer = m_startTime - msecsinceperiodstart;
                    }
                    else if (msecsinceperiodstart > finishtime)
                    {
                        newvalue  = false;
                        nexttimer = periodlength - msecsinceperiodstart;
                    }
                    else
                    {
                        // m_startDuration <= timesinceperiodstart <= finishtime
                        newvalue  = true;
                        nexttimer = finishtime - msecsinceperiodstart;
                    }
                }
                // start + duration = max
                // off/on _-
                else if (finishtime == periodlength)
                {
                    newvalue  = msecsinceperiodstart >= m_startTime;
                    nexttimer = newvalue ? periodlength - msecsinceperiodstart : m_startTime - msecsinceperiodstart;
                }
                // on/off/on -_-passthrough
                else if (finishtime > periodlength)
                {
                    quint64 firststart = finishtime - periodlength;
                    if (msecsinceperiodstart <= firststart)
                    {
                        newvalue  = true;
                        nexttimer = firststart - msecsinceperiodstart;
                    }
                    else if (msecsinceperiodstart >= m_startTime)
                    {
                        newvalue  = true;
                        nexttimer = periodlength - msecsinceperiodstart;
                    }
                    else
                    {
                        newvalue  = false;
                        nexttimer = m_startTime - msecsinceperiodstart;
                    }
                }

                if (first && newvalue)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Triggering timer '%1' late - will run for %2seconds instead of %3")
                        .arg(uniqueId).arg(nexttimer/1000.0).arg(m_duration/1000));
                }

                // trigger timer a little early to hone in and give sub-second accuracy
                quint64 adjust = nexttimer / 10;
                if (nexttimer > 100)
                    nexttimer -= adjust;

                // avoid lengthy timers - max 1 hour
                if (nexttimer > kMSecsInHour)
                    nexttimer = kMSecsInHour;

                SetValue(newvalue);
                m_timer.start(nexttimer);
                return;
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
    QMutexLocker locker(&lock);

    // Custom timers don't have a defined start time
    if (TorcTimerControl::Custom == m_timerType ||
        TorcTimerControl::UnknownTimerType == m_timerType)
        return 0;

    quint64 msecsinceperiodstart = MsecsSincePeriodStart();
    quint64 finishtime   = m_startTime + m_duration;
    quint64 periodlength = GetPeriodDuration();
    quint64 lasttimer    = 0;

    // N.B. There can only ever be two 'reference' transitions - the last low to high
    //      and the last high to low. A boundary is NOT a transition (unless coincidental).

    // start is zero
    // on/off -_
    if (m_startTime == 0)
    {
        lasttimer = (msecsinceperiodstart <= m_duration) ? msecsinceperiodstart : msecsinceperiodstart - m_duration;
    }
    // start + duration < max
    // off/on/off _-_
    else if (finishtime < periodlength)
    {
        if (msecsinceperiodstart < m_startTime)
        {
            lasttimer = msecsinceperiodstart + (periodlength - finishtime);
        }
        else if (msecsinceperiodstart > finishtime)
        {
            lasttimer = msecsinceperiodstart - finishtime;
        }
        else
        {
            lasttimer = msecsinceperiodstart - m_startTime;
        }
    }
    // start + duration = max
    // off/on _-
    else if (finishtime == periodlength)
    {
        lasttimer = (msecsinceperiodstart >= m_startTime) ? msecsinceperiodstart - m_startTime : msecsinceperiodstart;
    }
    // on/off/on -_-
    else if (finishtime > periodlength)
    {
        quint64 firststart = finishtime - periodlength;
        if (msecsinceperiodstart <= firststart)
        {
            lasttimer = msecsinceperiodstart + (periodlength - m_startTime);
        }
        else if (msecsinceperiodstart >= m_startTime)
        {
            lasttimer = msecsinceperiodstart - m_startTime;
        }
        else
        {
            lasttimer = msecsinceperiodstart - firststart;
        }
    }

    return lasttimer;
}

quint64 TorcTimerControl::MsecsSincePeriodStart(void)
{
    QTime timenow = QTime::currentTime();
    int       day = QDate::currentDate().dayOfWeek() - 1;

    // clip hours for Hourly
    if (TorcTimerControl::Hourly == m_timerType)
        timenow = QTime(0, timenow.minute(), timenow.second(), timenow.msec());
    // clip hours and minutes for minutely
    else if (TorcTimerControl::Minutely == m_timerType)
        timenow = QTime(0, 0, timenow.second(), timenow.msec());

    quint64 msecsinceperiodstart = timenow.msecsSinceStartOfDay();
    // add days for Weekly
    if (TorcTimerControl::Weekly == m_timerType)
        msecsinceperiodstart += day * kMSecsInDay;
    return msecsinceperiodstart;
}

quint64 TorcTimerControl::GetPeriodDuration(void) const
{
    switch (m_timerType)
    {
        case TorcTimerControl::Minutely: return kMSecsInMinute;
        case TorcTimerControl::Hourly:   return kMSecsInHour;
        case TorcTimerControl::Daily:    return kMSecsInDay;
        case TorcTimerControl::Weekly:   return kMSecsinWeek;
        default:
            break;
    }

    return 0;
}

void TorcTimerControl::CalculateOutput(void)
{
}
