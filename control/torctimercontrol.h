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

    TorcControl::Type GetType         (void);
    bool              Validate        (void);
    void              Start           (void);
    quint64           TimeSinceLastTransition (void);
    TorcTimerControl::TimerType GetTimerType  (void);

  public slots:
    void              TimerTimeout    (void);

  private:
    void              CalculateOutput (void);
    quint64           GetMaxDuration  (void);

  private:
    TorcTimerControl::TimerType m_timerType;

    QTime             m_startTime;
    int               m_startDay;
    quint64           m_startDuration;

    quint64           m_duration;
    QTime             m_durationTime;
    int               m_durationDay;

    QTimer           *m_timer;
    bool              m_firstTrigger;
};

#endif // TORCTIMERCONTROL_H
