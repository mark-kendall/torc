#ifndef TORCNOTIFICATION_H
#define TORCNOTIFICATION_H

// Torc
#include "torcdevice.h"
#include "torcnotifier.h"

class TorcNotification : public TorcDevice
{
    Q_OBJECT

  public:
    TorcNotification(const QVariantMap &Details);
    virtual bool         Setup   (void);

  signals:
    void                 Notify  (const QVariantMap &Message);

  protected:
    virtual ~TorcNotification();

  protected:
    QStringList          m_notifierNames;
    QList<TorcNotifier*> m_notifiers;
    QString              m_title;
    QString              m_body;
};

class TorcNotificationFactory
{
  public:
    TorcNotificationFactory();
    virtual ~TorcNotificationFactory();

    static TorcNotificationFactory*   GetTorcNotificationFactory (void);
    TorcNotificationFactory*          NextFactory                (void) const;
    virtual TorcNotification*         Create                     (const QString &Type, const QVariantMap &Details) = 0;

  protected:
    static TorcNotificationFactory*   gTorcNotificationFactory;
    TorcNotificationFactory*          nextTorcNotificationFactory;
};

#endif // TORCNOTIFICATION_H
