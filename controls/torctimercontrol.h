#ifndef TORCTIMERCONTROL_H
#define TORCTIMERCONTROL_H

// Qt
#include <QTime>
#include <QTimer>

// Torc
#include "torccontrol.h"

class TorcTimerControl : public TorcControl
{
    Q_OBJECT

  public:
    enum TimerType
    {
        UnknownTimerType,
        Custom,
        Minutely, // yes, small in time...
        Hourly,
        Daily,
        Weekly
    };

    static QString  TimerTypeToString (TorcTimerControl::TimerType Type);
    static TorcTimerControl::TimerType StringToTimerType (const QString &Type);

  public:
    TorcTimerControl(const QString &Type, const QVariantMap &Details);
   ~TorcTimerControl();

    TorcControl::Type GetType         (void) const Q_DECL_OVERRIDE;
    QStringList       GetDescription  (void) Q_DECL_OVERRIDE;
    bool              Validate        (void) Q_DECL_OVERRIDE;
    void              Start           (void) Q_DECL_OVERRIDE;
    bool              AllowInputs     (void) const Q_DECL_OVERRIDE;
    quint64           TimeSinceLastTransition (void);
    TorcTimerControl::TimerType GetTimerType  (void) const;

  public slots:
    void              TimerTimeout    (void);
    bool              event           (QEvent *Event) Q_DECL_OVERRIDE;

  private:
    void              GenerateTimings (void);
    void              CalculateOutput (void) Q_DECL_OVERRIDE;
    quint64           GetPeriodDuration(void) const;
    quint64           MsecsSincePeriodStart (void);

  private:
    TorcTimerControl::TimerType m_timerType;
    int               m_startDay;
    quint64           m_startTime;
    quint64           m_duration;
    int               m_durationDay;
    int               m_periodDay;
    quint64           m_periodTime;
    QTimer            m_timer;
    bool              m_firstTrigger;
    bool              m_randomStart;
    bool              m_randomDuration;
    quint64           m_lastElapsed;
    bool              m_newRandom;
};

#endif // TORCTIMERCONTROL_H
