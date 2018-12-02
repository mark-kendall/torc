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
    Q_PROPERTY(double animationValue READ GetAnimationValue() WRITE SetAnimationValue(Value) CONSTANT)

  public:
    TorcTransitionControl(const QString &Type, const QVariantMap &Details);
   ~TorcTransitionControl();

    static QEasingCurve::Type EasingCurveFromString (const QString &Curve);
    static QString            StringFromEasingCurve (QEasingCurve::Type Type);
    bool                      Validate              (void) override;
    TorcControl::Type         GetType               (void) const override;
    QStringList               GetDescription        (void) override;

    static qreal              LinearLEDFunction     (qreal progress);

  public slots:
    bool                      event                 (QEvent *Event) override;
    void                      Restart               (void);
    void                      SetAnimationValue     (double Value);
    double                    GetAnimationValue     (void);

  private:
    void                      CalculateOutput       (void) override;

  private:
    quint64                   m_duration;
    QEasingCurve::Type        m_type;
    QPropertyAnimation        m_animation;
    double                    animationValue;
    bool                      m_firstTrigger;
    double                    m_transitionValue; // tracks the input value to filter transitions
};

#endif // TORCTRANSITIONCONTROL_H
