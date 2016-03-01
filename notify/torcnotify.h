#ifndef TORCNOTIFY_H
#define TORCNOTIFY_H

// Qt
#include <QVariant>

// Torc
#include "torcdevicehandler.h"
#include "torcnotification.h"
#include "torcnotifier.h"

class TorcNotifier;

class TorcNotify : public QObject, public TorcDeviceHandler
{
    Q_OBJECT

  public:
    static TorcNotify *gNotify;

  public:
    TorcNotify();
    ~TorcNotify();

    bool          Validate               (void) const;
    TorcNotifier* FindNotifierByName     (const QString &Name) const;
    QVariantMap   SetNotificationText    (const QString &Title, const QString &Body, const QMap<QString,QString> &Custom);
    void          Graph                  (QByteArray* Data);

  public slots:
    void          ApplicationNameChanged (void);

  protected:
    void          Create                 (const QVariantMap &Details);
    void          Destroy                (void);

  private:
    QList<TorcNotifier*>                 m_notifiers;
    QList<TorcNotification*>             m_notifications;
    bool                                 m_applicationNameChanged;
};
#endif // TORCNOTIFY_H
