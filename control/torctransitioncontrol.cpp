/* Class TorcTransitionControl
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
#include <QVariant>

// Torc
#include "torclogging.h"
#include "torctimercontrol.h"
#include "torctransitioncontrol.h"

QEasingCurve::Type TorcTransitionControl::EasingCurveFromString(const QString &Curve)
{
    QString curve = Curve.trimmed().toUpper();

    // may as well do them all:)
    if ("LINEAR"       == curve) return QEasingCurve::Linear;
    if ("INQUAD"       == curve) return QEasingCurve::InQuad;
    if ("OUTQUAD"      == curve) return QEasingCurve::OutQuad;
    if ("INOUTQUAD"    == curve) return QEasingCurve::InOutQuad;
    if ("OUTINQUAD"    == curve) return QEasingCurve::OutInQuad;
    if ("INCUBIC"      == curve) return QEasingCurve::InCubic;
    if ("OUTCUBIC"     == curve) return QEasingCurve::OutCubic;
    if ("INOUTCUBIC"   == curve) return QEasingCurve::InOutCubic;
    if ("OUTINCUBIC"   == curve) return QEasingCurve::OutInCubic;
    if ("INQUART"      == curve) return QEasingCurve::InQuart;
    if ("OUTQUART"     == curve) return QEasingCurve::OutQuart;
    if ("INOUTQUART"   == curve) return QEasingCurve::InOutQuart;
    if ("OUTINQUART"   == curve) return QEasingCurve::OutInQuart;
    if ("INQUINT"      == curve) return QEasingCurve::InQuint;
    if ("OUTQUINT"     == curve) return QEasingCurve::OutQuint;
    if ("INOUTQUINT"   == curve) return QEasingCurve::InOutQuint;
    if ("OUTINQUINT"   == curve) return QEasingCurve::OutInQuint;
    if ("INSINE"       == curve) return QEasingCurve::InSine;
    if ("OUTSINE"      == curve) return QEasingCurve::OutSine;
    if ("INOUTSINE"    == curve) return QEasingCurve::InOutSine;
    if ("OUTINSINE"    == curve) return QEasingCurve::OutInSine;
    if ("INEXPO"       == curve) return QEasingCurve::InExpo;
    if ("OUTEXPO"      == curve) return QEasingCurve::OutExpo;
    if ("INOUTEXPO"    == curve) return QEasingCurve::InOutExpo;
    if ("OUTINEXPO"    == curve) return QEasingCurve::OutInExpo;
    if ("INCIRC"       == curve) return QEasingCurve::InCirc;
    if ("OUTCIRC"      == curve) return QEasingCurve::OutCirc;
    if ("INOUTCIRC"    == curve) return QEasingCurve::InOutCirc;
    if ("OUTINCIRC"    == curve) return QEasingCurve::OutInCirc;
    if ("INELASTIC"    == curve) return QEasingCurve::InElastic;
    if ("OUTELASTIC"   == curve) return QEasingCurve::OutElastic;
    if ("INOUTELASTIC" == curve) return QEasingCurve::InOutElastic;
    if ("OUTINELASTIC" == curve) return QEasingCurve::OutInElastic;
    if ("INBACK"       == curve) return QEasingCurve::InBack;
    if ("OUTBACK"      == curve) return QEasingCurve::OutBack;
    if ("INOUTBACK"    == curve) return QEasingCurve::InOutBack;
    if ("OUTINBACK"    == curve) return QEasingCurve::OutInBack;
    if ("INBOUNCE"     == curve) return QEasingCurve::InBounce;
    if ("OUTBOUNCE"    == curve) return QEasingCurve::OutBounce;
    if ("INOUTBOUNCE"  == curve) return QEasingCurve::InOutBounce;
    if ("OUTINBOUNCE"  == curve) return QEasingCurve::OutInBounce;

    LOG(VB_GENERAL, LOG_WARNING, QString("Failed to parse transition type from '%1'").arg(Curve));
    return QEasingCurve::Custom; // invalid
}

QString TorcTransitionControl::StringFromEasingCurve(QEasingCurve::Type Type)
{
    switch (Type)
    {
        case QEasingCurve::Linear:       return tr("Linear");
        case QEasingCurve::InQuad:       return tr("InQuad");
        case QEasingCurve::OutQuad:      return tr("OutQuad");
        case QEasingCurve::InOutQuad:    return tr("InOutQuad");
        case QEasingCurve::OutInQuad:    return tr("OutInQuad");
        case QEasingCurve::InCubic:      return tr("InCubic");
        case QEasingCurve::OutCubic:     return tr("OutCubic");
        case QEasingCurve::InOutCubic:   return tr("InOutCubic");
        case QEasingCurve::OutInCubic:   return tr("OutInCubic");
        case QEasingCurve::InQuart:      return tr("InQuart");
        case QEasingCurve::OutQuart:     return tr("OutQuart");
        case QEasingCurve::InOutQuart:   return tr("InOutQuart");
        case QEasingCurve::OutInQuart:   return tr("OutInQuart");
        case QEasingCurve::InQuint:      return tr("InQuint");
        case QEasingCurve::OutQuint:     return tr("OutQuint");
        case QEasingCurve::InOutQuint:   return tr("InOutQuint");
        case QEasingCurve::OutInQuint:   return tr("OutInQuint");
        case QEasingCurve::InSine:       return tr("InSine");
        case QEasingCurve::OutSine:      return tr("OutSine");
        case QEasingCurve::InOutSine:    return tr("InOutSine");
        case QEasingCurve::OutInSine:    return tr("OutInSine");
        case QEasingCurve::InExpo:       return tr("InExpo");
        case QEasingCurve::OutExpo:      return tr("OutExpo");
        case QEasingCurve::InOutExpo:    return tr("InOutExpo");
        case QEasingCurve::OutInExpo:    return tr("OutInExpo");
        case QEasingCurve::InCirc:       return tr("InCirc");
        case QEasingCurve::OutCirc:      return tr("OutCirc");
        case QEasingCurve::InOutCirc:    return tr("InOutCirc");
        case QEasingCurve::OutInCirc:    return tr("OutInCirc");
        case QEasingCurve::InElastic:    return tr("InElastic");
        case QEasingCurve::OutElastic:   return tr("OutElastic");
        case QEasingCurve::InOutElastic: return tr("InOutElastic");
        case QEasingCurve::OutInElastic: return tr("OutInElastic");
        case QEasingCurve::InBack:       return tr("InBack");
        case QEasingCurve::OutBack:      return tr("OutBack");
        case QEasingCurve::InOutBack:    return tr("InOutBack");
        case QEasingCurve::OutInBack:    return tr("OutInBack");
        case QEasingCurve::InBounce:     return tr("InBounce");
        case QEasingCurve::OutBounce:    return tr("OutBounce");
        case QEasingCurve::InOutBounce:  return tr("InOutBounce");
        case QEasingCurve::OutInBounce:  return tr("OutInBounce");
        default:
            break;
    }

    return tr("Unknown");
}

TorcTransitionControl::TorcTransitionControl(const QString &Type, const QVariantMap &Details)
  : TorcControl(TorcControl::Transition, Details),
    m_duration(0),
    m_type(QEasingCurve::Linear),
    m_animation(new QPropertyAnimation(this, "animationValue")),
    animationValue(0),
    m_firstTrigger(true),
    m_transitionValue(0)
{
    // determine curve
    m_type = EasingCurveFromString(Type);
    if (m_type == QEasingCurve::Custom /*invalid*/)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unknown transition type '%1' for device '%2'").arg(Type).arg(uniqueId));
        return;
    }

    // check duration
    if (!Details.contains("duration"))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Transition '%1' does not specify duration").arg(uniqueId));
        return;
    }

    int days, hours, minutes, seconds;
    QString durations = Details.value("duration").toString();
    quint64 duration;
    if (!TorcControl::ParseTimeString(durations, days, hours, minutes, seconds, duration))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Failed to parse transition duration from '%1'").arg(durations));
        return;
    }

    // no point in having a zero length transition
    if (duration < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Transition duration is invalid ('%1')").arg(duration));
        return;
    }
    m_duration = duration;

    // so far so good
    m_parsed = true;
}

TorcTransitionControl::~TorcTransitionControl()
{
    delete m_animation;
}

TorcControl::Type TorcTransitionControl::GetType(void)
{
    return TorcControl::Transition;
}

QStringList TorcTransitionControl::GetDescription(void)
{
    static const quint64 secondsinday = 60 * 60 * 24;
    int     daysduration = m_duration / secondsinday;
    quint64 timeduration = m_duration % secondsinday;

    QStringList result;
    result.append(tr("%1 transition").arg(StringFromEasingCurve(m_type)));
    result.append(tr("Duration %1").arg(TorcControl::DurationToString(daysduration, timeduration)));
    return result;
}

bool TorcTransitionControl::Validate(void)
{
    QMutexLocker locker(lock);

    // don't repeat validation
    if (m_validated)
        return true;

    // common checks
    if (!TorcControl::Validate())
        return false;

    // need one input and one input only
    if (m_inputs.size() != 1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Transition device '%1' needs exactly one input").arg(uniqueId));
        return false;
    }

    // need at least one output
    if (m_outputs.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Transition device '%1' needs at least one output").arg(uniqueId));
        return false;
    }

    // if we get this far, we can finish the device
    if (!Finish())
        return false;

    // setup the animation now
    QEasingCurve easingcurve(m_type);
    m_animation->setEasingCurve(easingcurve);
    m_animation->setStartValue(0);
    m_animation->setEndValue(1);
    m_animation->setDuration(m_duration * 1000);

    // debug
    LOG(VB_GENERAL, LOG_INFO, QString("%1: %2").arg(uniqueId).arg(GetDescription().join(",")));

    return true;
}

/*! \fn CalculateOutput
 *
 * This function is called when the input is valid and the input validity and/or the value
 * have just changed. We need to decide whether it is a rising or falling transition and
 * start the transition as appropriate. Special care needs to be taken with timer inputs
 * at startup to ensure we start with the correct value for the given time and the animation
 * is running in the correct direction.
 *
 * /note The output value is effectively disconnected from the input state and cannot be
 * use to validate the input. Hence we use m_transitionState to track the input state and
 * filter extra updates - which can happen at startup. Additional events, if unfiltered, restart the animation.
*/
void TorcTransitionControl::CalculateOutput(void)
{
    QMutexLocker locker(lock);

    // sanity check
    if (m_inputs.size() != 1)
        return;

    quint64 timesincelasttransition = 0;

    // If the input is a timer, at startup we need to check the status of the timer and the time
    // elapsed since its last transition. We can then force the animation output.
    // This handles, for example, a light dimmer that is configured for a 1 hour sunrise/sunset.
    // If we need to turn the system off during the day, we don't want the lights to take another hour
    // to turn back on - but to move to the value we would expect for the time of day. This could
    // perhaps be configurable (some people might want them to take the hour) but sudden transitions
    // could also be 'smoothed' with a seperate transition.

    double newvalue = m_inputValues.constBegin().value();
    LOG(VB_GENERAL, LOG_INFO, QString("Transition value: %1").arg(newvalue));

    if (m_firstTrigger)
    {
        m_firstTrigger = false;
        m_transitionValue = newvalue;

        TorcTimerControl *timerinput = qobject_cast<TorcTimerControl*>(m_inputs.firstKey());
        if (timerinput)
        {
            // NB for a customer timer with transition, this will force the output to 'on' initially.
            // While this may seem counterintuitive, it is the expected behaviour - as the output
            // would normally be transitioning from the last 'on' to 'off' at time 0. Changing
            // the custom timer behaviour would produce unexpected results for timers with no transition.
            timesincelasttransition = timerinput->TimeSinceLastTransition();

            if (timesincelasttransition > m_duration)
            {
                LOG(VB_GENERAL, LOG_INFO, QString("Transition '%1' is initially inactive (value '%2')").arg(uniqueId).arg(newvalue));
                SetValue(newvalue);
                return;
            }

            // if we are part way through the transition, the animation will expect the value to have started
            // from the previous transition value !:)
            SetValue(newvalue > 0 ? 0 : 1);
            LOG(VB_GENERAL, LOG_INFO, QString("Forcing transition '%1' time to %2% complete").arg(uniqueId)
                .arg(((double)timesincelasttransition / (double)m_duration) * 100.0));

            if (newvalue < 1) // time can run backwards :)
                timesincelasttransition = m_duration - timesincelasttransition;
        }
    }
    else
    {
        if (qFuzzyCompare(newvalue + 1.0, m_transitionValue + 1.0))
            return;
    }

    m_transitionValue = newvalue;

    // We assume the specified easing curve is for a rising (0-1) transition. We then run it in
    // reverse for the 'mirrored' falling operation.
    // NB if the animation is still running, it will reverse from its current position - so there
    // will be no glitches/jumps/interruptions.
    m_animation->setDirection(newvalue > 0 ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    m_animation->start();

    // this has to come after start
    if (timesincelasttransition > 0)
        m_animation->setCurrentTime(timesincelasttransition * 1000);
}

/*! \fn SetAnimationValue
 *
 * Our main output, value, is read only. So the animation operates on a proxy, animationValue.
*/
void TorcTransitionControl::SetAnimationValue(double Value)
{
    QMutexLocker locker(lock);

    animationValue = Value;
    SetValue(Value);
}

double TorcTransitionControl::GetAnimationValue(void)
{
    QMutexLocker locker(lock);
    return animationValue;
}
