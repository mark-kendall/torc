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
        case TorcTimerControl::Minutely: return QString("Minute");
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
    m_periodDay(0),
    m_periodTime(0),
    m_timer(),
    m_firstTrigger(true),
    m_randomStart(false),
    m_randomDuration(false),
    m_lastElapsed(0),
    m_newRandom(false)
{
    static bool randomcheck = true;
    if (randomcheck)
    {
        randomcheck = false;
        if (RAND_MAX < kMSecsinWeek)
            LOG(VB_GENERAL, LOG_WARNING, QString("Maximum random number is too low for effective use (%1)").arg(RAND_MAX));
    }

    if (m_timerType == TorcTimerControl::UnknownTimerType)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown timer type '%1' for device '%2'").arg(Details.value("type").toString()).arg(uniqueId));
        return;
    }

    int days, hours, minutes, seconds = 0;

    // a custom timer needs a period
    if (TorcTimerControl::Custom == m_timerType)
    {
        if (!Details.contains("period"))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Custom timer %1 does not specify a period").arg(uniqueId));
            return;
        }

        QString periods = Details.value("period").toString().toLower().trimmed();
        quint64 periodtime;
        if (!TorcControl::ParseTimeString(periods, days, hours, minutes, seconds, periodtime))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse period from '%1'").arg(periods));
            return;
        }

        m_periodDay  = days;
        m_periodTime = periodtime * 1000;

        // the period must be at least 2 seconds to allow for defined on and off periods
        if (m_periodTime < 2000)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Custom timer %1 has duration of %2seconds - needs at least 2").arg(uniqueId).arg(m_periodTime / 1000));
            return;
        }
    }
    else
    {
        m_periodTime = GetPeriodDuration();
        m_periodDay  = m_periodTime / kMSecsInDay;
    }

    // Sanity checks start time and duration
    // constraints:
    // - we need a defined off and on period
    // - the on period can be anywhere in the cycle (start, middle, end)
    // - the current on period must finish before the next is due to start
    // - so:
    //   - start >= 0
    //   - start < max
    //   - duration > 0
    //   - duration < max
    // - should cover all eventualities...

    // check for start time
    if (!Details.contains("start"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Timer '%1' does not specify start time").arg(uniqueId));
        return;
    }

    // a random start time
    QString start  = Details.value("start").toString().toLower().trimmed();
    if (start.contains("random"))
    {
        m_randomStart = true;
    }
    else
    {
        // start time must be in the format DD:HH:MM or HH:MM or MM with an optional .SS appended
        quint64 starttime;
        if (!TorcControl::ParseTimeString(start, days, hours, minutes, seconds, starttime))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse start time from '%1'").arg(start));
            return;
        }

        m_startDay  = days;
        m_startTime = starttime * 1000;

        if (m_startTime >= m_periodTime /*|| m_startTime < 0*/)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Start time (%2) for %1 is invalid - must be in range 0-%3")
                .arg(uniqueId).arg(m_startTime / 1000).arg((m_periodTime / 1000) - 1));
            return;
        }
    }

    // check duration - format as for start time
    if (!Details.contains("duration"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Timer '%1' does not specify duration").arg(uniqueId));
        return;
    }

    QString durations = Details.value("duration").toString().toLower().trimmed();
    if (durations.contains("random"))
    {
        // a timer cannot have random start and duration
        if (m_randomStart)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Timer %1 cannot have random start AND duration").arg(uniqueId));
            return;
        }
        m_randomDuration = true;
    }
    else
    {
        quint64 duration;
        if (!TorcControl::ParseTimeString(durations, days, hours, minutes, seconds, duration))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse duration from '%1'").arg(durations));
            return;
        }

        m_duration    = duration * 1000;
        m_durationDay = days;

        if ((m_duration < 1000) || (m_duration >= m_periodTime))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Duration (%1) for %2 is invalid - must be in range 1-%3")
                .arg(m_duration / 1000).arg(uniqueId).arg((m_periodTime / 1000) - 1));
            return;
        }
    }

    // generate random start/duration if necessary
    GenerateTimings();

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

    result.append(TimerTypeToString(m_timerType));
    result.append(tr("Period") + ": " + TorcControl::DurationToString(m_periodDay, m_periodTime / 1000));
    result.append(tr("Start") + ": " + (m_randomStart    ? tr("Random") : TorcControl::DurationToString(m_startDay, m_startTime / 1000)));
    result.append(tr("Duration") + ": " + (m_randomDuration ? tr("Random") : TorcControl::DurationToString(m_durationDay, m_duration / 1000)));
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
        case TorcTimerControl::Minutely:
        case TorcTimerControl::Hourly:
        case TorcTimerControl::Daily:
        case TorcTimerControl::Weekly:
            // we need to establish the current time, the current state
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
                bool    newrandom    = false;
                bool    newvalue     = false;
                quint64 nexttimer    = 0;

                // start is zero
                // on/off -_
                if (m_startTime == 0)
                {
                    newvalue  = msecsinceperiodstart <= m_duration;
                    nexttimer = newvalue ? m_duration - msecsinceperiodstart : m_periodTime - msecsinceperiodstart;
                    newrandom = value < 1.0 && newvalue; // transitioning from low to high
                }
                // start + duration < max
                // off/on/off _-_
                else if (finishtime < m_periodTime)
                {
                    if (msecsinceperiodstart < m_startTime)
                    {
                        newvalue  = false;
                        nexttimer = m_startTime - msecsinceperiodstart;
                    }
                    else if (msecsinceperiodstart > finishtime)
                    {
                        newvalue  = false;
                        nexttimer = m_periodTime - msecsinceperiodstart;
                    }
                    else
                    {
                        // m_startTime <= timesinceperiodstart <= finishtime
                        newvalue  = true;
                        nexttimer = finishtime - msecsinceperiodstart;
                    }

                    if (!newvalue && (value < 1.0) && (m_lastElapsed > msecsinceperiodstart)) // end of period boundary
                        newrandom = true;
                }
                // start + duration = max
                // off/on _-
                else if (finishtime == m_periodTime)
                {
                    newvalue  = msecsinceperiodstart >= m_startTime;
                    nexttimer = newvalue ? m_periodTime - msecsinceperiodstart : m_startTime - msecsinceperiodstart;
                    newrandom = value > 0.0 && !newvalue; // transitioning from high to low
                }
                // on/off/on -_-
                else if (finishtime > m_periodTime)
                {
                    // a random timer should never hit this condition as it becomes too involved
                    // trying to trigger the correct timeouts
                    if (m_randomDuration || m_randomStart)
                    {
                        LOG(VB_GENERAL, LOG_ERR, "Invalid condition for random timer - disabling");
                        return;
                    }

                    quint64 firststart = finishtime - m_periodTime;

                    if (msecsinceperiodstart <= firststart)
                    {
                        newvalue  = true;
                        nexttimer = firststart - msecsinceperiodstart;
                    }
                    else if (msecsinceperiodstart >= m_startTime)
                    {
                        newvalue  = true;
                        nexttimer = m_periodTime - msecsinceperiodstart;
                    }
                    else
                    {
                        newvalue  = false;
                        nexttimer = m_startTime - msecsinceperiodstart;
                    }

                }

                if (first && newvalue)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString("Triggering timer '%1' late - will run for %2seconds instead of %3 %4")
                        .arg(uniqueId).arg(nexttimer/1000.0).arg(m_duration/1000).arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")));
                }

                m_lastElapsed = msecsinceperiodstart;

                // the end of the 'random' sequence. Calculate next sequence and then
                // recalculate next timeout. Don't set the value as it may momentarily be set to the incorrect value.
                // m_newRandom prevents a potential infinite loop as a new random start/duration may immediately
                // trigger another. Don't reset random on first trigger - it has only just been set!
                if (!first && newrandom && !m_newRandom && (m_randomDuration || m_randomStart))
                {
                    m_newRandom = true;
                    GenerateTimings();
                    TimerTimeout();
                    return;
                }

                m_newRandom = false;

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
    else if (finishtime < m_periodTime)
    {
        if (msecsinceperiodstart < m_startTime)
        {
            lasttimer = msecsinceperiodstart + (m_periodTime - finishtime);
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
    else if (finishtime == m_periodTime)
    {
        lasttimer = (msecsinceperiodstart >= m_startTime) ? msecsinceperiodstart - m_startTime : msecsinceperiodstart;
    }
    // on/off/on -_-
    else if (finishtime > m_periodTime)
    {
        quint64 firststart = finishtime - m_periodTime;
        if (msecsinceperiodstart <= firststart)
        {
            lasttimer = msecsinceperiodstart + (m_periodTime - m_startTime);
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

    if (TorcTimerControl::Custom == m_timerType)
    {
        // always use the same reference Date/Time to ensure long duration custom timers start consistently
        // and to prevent drift
        static const QDateTime reference = QDateTime::fromString("2000-01-01T00:00:00", Qt::ISODate);
        if (m_periodTime > 0)
            return (QDateTime::currentMSecsSinceEpoch() - reference.toMSecsSinceEpoch()) % m_periodTime;
        return 0;
    }

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

/*! \brief Generate random start or duration for timers.
*   \note Start or duration must be validated before calling this function.
*/
void TorcTimerControl::GenerateTimings(void)
{
    // start or duration are limited in range as we must keep the cycle within the period
    if (m_randomDuration)
    {
        // for a 60 second timer, we need duration between 1 and 59 - a range of 58
        // start is already 0-59 - a range of 59...
        // if we clip the result, we lose the random distribution
        // so we need to scale down the start time to restrict the range
        // which means we need to operate to ms accuracy and round down
        quint64 start = (quint64)((double)m_startTime * ((double)(m_periodTime - 1000) / (double)m_periodTime));
        quint64 mod   = (m_periodTime / 1000) - (start / 1000); // range 2<->period
                mod  -= 1; // range 1<->(period-1)
        m_duration    = (qrand() % mod); // range 0<->period-2
        m_duration   += 1; // range 1<->period-1
        m_duration   *= 1000;
        m_durationDay = m_duration / kMSecsInDay;
        LOG(VB_GENERAL, LOG_INFO, QString("Timer %1 - new random duration %2").arg(uniqueId).arg(DurationToString(m_durationDay, m_duration / 1000)));
    }
    else if (m_randomStart)
    {
        quint64 mod = (m_periodTime / 1000) - (m_duration / 1000) + 1; // range 2<->period
        m_startTime = qrand() % (mod); // range 0<->period-1
        m_startTime *= 1000;
        m_startDay = m_startTime / kMSecsInDay;
        LOG(VB_GENERAL, LOG_INFO, QString("Timer %1 - new random start %2").arg(uniqueId).arg(DurationToString(m_startDay, m_startTime / 1000)));
    }
}

void TorcTimerControl::CalculateOutput(void)
{
}
