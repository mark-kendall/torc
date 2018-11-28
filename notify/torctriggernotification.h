#ifndef TORCTRIGGERNOTIFICATION_H
#define TORCTRIGGERNOTIFICATION_H

// Torc
#include "torcnotification.h"

class TorcDevice;

class TorcTriggerNotification final : public TorcNotification
{
    Q_OBJECT

  public:
    explicit TorcTriggerNotification(const QVariantMap &Details);
    ~TorcTriggerNotification();
    bool         IsKnownInput      (const QString &UniqueId) override;
    QStringList  GetDescription    (void) override;
    void         Graph             (QByteArray *Data) override;

  public slots:
    void         InputValueChanged (double Value);

  protected:
    virtual bool Setup             (void) override;

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
