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
        Weekly,
        SingleShot
    };

    static QString  TimerTypeToString (TorcTimerControl::TimerType Type);
    static TorcTimerControl::TimerType StringToTimerType (const QString &Type);

  public:
    TorcTimerControl(const QString &Type, const QVariantMap &Details);
   ~TorcTimerControl();

    TorcControl::Type GetType         (void) const override;
    QStringList       GetDescription  (void) override;
    bool              Validate        (void) override;
    void              Start           (void) override;
    bool              AllowInputs     (void) const override;
    quint64           TimeSinceLastTransition (void);
    TorcTimerControl::TimerType GetTimerType  (void) const;

  public slots:
    void              TimerTimeout    (void);
    bool              event           (QEvent *Event) override;

  private:
    void              GenerateTimings (void);
    void              CalculateOutput (void) override;
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
    bool              m_active;
    quint64           m_singleShotStartTime;
};

#endif // TORCTIMERCONTROL_H
