#ifndef TORCPWMOUTPUT_H
#define TORCPWMOUTPUT_H

// Torc
#include "torcoutput.h"

#define DEFAULT_PWM_RESOLUTION 1024 // 10bits

class TorcPWMOutput : public TorcOutput
{
    Q_OBJECT
    Q_PROPERTY(uint Resolution    MEMBER m_resolution    READ GetResolution CONSTANT)
    Q_PROPERTY(uint maxResolution MEMBER m_maxResolution READ GetMaxResolution CONSTANT)

  public:
    TorcPWMOutput(double Value, const QString &ModelId, const QVariantMap &Details, uint MaxResolution = DEFAULT_PWM_RESOLUTION);
    virtual ~TorcPWMOutput();

    QStringList      GetDescription   (void) Q_DECL_OVERRIDE;
    TorcOutput::Type GetType          (void) Q_DECL_OVERRIDE;

  public slots:
    uint             GetResolution    (void);
    uint             GetMaxResolution (void);

  protected:
    bool             ValueIsDifferent (double &NewValue);

  protected:
    uint             m_resolution;
    uint             m_maxResolution;
};

#endif // TORCPWMOUTPUT_H
