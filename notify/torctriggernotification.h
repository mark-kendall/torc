#ifndef TORCTRIGGERNOTIFICATION_H
#define TORCTRIGGERNOTIFICATION_H

// Torc
#include "torcnotification.h"

class TorcDevice;

class TorcTriggerNotification : public TorcNotification
{
    Q_OBJECT

  public:
    explicit TorcTriggerNotification(const QVariantMap &Details);
    ~TorcTriggerNotification();
    bool         IsKnownInput      (const QString &UniqueId) Q_DECL_OVERRIDE;
    QStringList  GetDescription    (void) Q_DECL_OVERRIDE;
    void         Graph             (QByteArray *Data) Q_DECL_OVERRIDE;

  public slots:
    void         InputValueChanged (double Value);

  protected:
    virtual bool Setup             (void) Q_DECL_OVERRIDE;

  private:
    QString      m_inputName;
    TorcDevice*  m_input;
    double       m_lastValue;
    bool         m_triggerHigh;
    QMap<QString,QString> m_customData;
    QStringList  m_references;
    QList<TorcDevice*> m_referenceDevices;

  private:
    Q_DISABLE_COPY(TorcTriggerNotification)
};

#endif // TORCTRIGGERNOTIFICATION_H
