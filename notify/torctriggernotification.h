#ifndef TORCTRIGGERNOTIFICATION_H
#define TORCTRIGGERNOTIFICATION_H

// Torc
#include "torcnotification.h"

class TorcDevice;

class TorcTriggerNotification : public TorcNotification
{
    Q_OBJECT

  public:
    TorcTriggerNotification(const QVariantMap &Details);
    ~TorcTriggerNotification();
    bool         IsKnownInput      (const QString &UniqueId) const;
    QStringList  GetDescription    (void);
    void         Graph             (QByteArray *Data);

  public slots:
    void         InputValueChanged (double Value);

  protected:
    virtual bool Setup             (void);

  private:
    QString      m_inputName;
    TorcDevice*  m_input;
    double       m_lastValue;
    bool         m_triggerHigh;
    QMap<QString,QString> m_customData;
    QStringList  m_references;
    QList<TorcDevice*> m_referenceDevices;
};

#endif // TORCTRIGGERNOTIFICATION_H
