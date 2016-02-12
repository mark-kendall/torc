#ifndef TORCNOTIFIER_H
#define TORCNOTIFIER_H

// Qt
#include <QVariant>

// Torc
#include "torcdevice.h"

#define NOTIFICATION_TITLE   QString("title")
#define NOTIFICATION_BODY    QString("body")
#define UNKNOWN_TITLE        QString("Torc")
#define UNKNOWN_BODY         QString("Unknown")

class TorcNotifier : public TorcDevice
{
    Q_OBJECT

  public slots:
    // QObject
    virtual bool event            (QEvent *e);
    virtual void Notify           (const QVariantMap &Notification) = 0;

  protected:
    TorcNotifier(const QVariantMap &Details);
    virtual ~TorcNotifier();
};

class TorcNotifierFactory
{
  public:
    TorcNotifierFactory();
    virtual ~TorcNotifierFactory();

    static TorcNotifierFactory*   GetTorcNotifierFactory (void);
    TorcNotifierFactory*          NextFactory            (void) const;
    virtual TorcNotifier*         Create                 (const QString &Type, const QVariantMap &Details) = 0;

  protected:
    static TorcNotifierFactory*   gTorcNotifierFactory;
    TorcNotifierFactory*          nextTorcNotifierFactory;
};
#endif // TORCNOTIFIER_H