#ifndef TORCNOTIFICATION_H
#define TORCNOTIFICATION_H

// Torc
#include "torcdevice.h"
#include "torcnotifier.h"

class TorcNotification : public TorcDevice
{
    Q_OBJECT

  public:
    explicit TorcNotification(const QVariantMap &Details);
    virtual bool         Setup        (void);
    virtual bool         IsKnownInput (const QString &UniqueId);
    virtual QStringList  GetDescription (void) = 0;
    virtual void         Graph        (QByteArray* Data) = 0;

  signals:
    void                 Notify       (const QVariantMap &Message);

  protected:
    virtual ~TorcNotification() = default;

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
    virtual ~TorcNotificationFactory() = default;

    static TorcNotificationFactory*   GetTorcNotificationFactory (void);
    TorcNotificationFactory*          NextFactory                (void) const;
    virtual TorcNotification*         Create                     (const QString &Type, const QVariantMap &Details) = 0;

  protected:
    static TorcNotificationFactory*   gTorcNotificationFactory;
    TorcNotificationFactory*          nextTorcNotificationFactory;

  private:
    Q_DISABLE_COPY(TorcNotificationFactory)
};

#endif // TORCNOTIFICATION_H
