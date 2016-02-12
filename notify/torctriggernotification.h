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
};

#endif // TORCTRIGGERNOTIFICATION_H
