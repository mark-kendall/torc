#ifndef TORCTRANSITIONCONTROL_H
#define TORCTRANSITIONCONTROL_H

// Qt
#include <QEasingCurve>
#include <QPropertyAnimation>

// Torc
#include "torccontrol.h"

class TorcTransitionControl : public TorcControl
{
    Q_OBJECT
    Q_PROPERTY(double animationValue READ GetAnimationValue() WRITE SetAnimationValue())

  public:
    TorcTransitionControl(const QVariantMap &Details);
   ~TorcTransitionControl();

    static QEasingCurve::Type EasingCurveFromString (const QString &Curve);
    bool                      Validate              (void);
    TorcControl::Type         GetType               (void);

  public slots:
    void                      SetAnimationValue    (double Value);
    double                    GetAnimationValue    (void);

  private:
    void                      CalculateOutput       (void);

  private:
    quint64                   m_duration;
    QEasingCurve::Type        m_type;
    QPropertyAnimation       *m_animation;
    double                    animationValue;
    bool                      m_firstTrigger;
    double                    m_transitionValue; // tracks the input value to filter transitions
};

#endif // TORCTRANSITIONCONTROL_H
