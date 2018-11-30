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
    virtual void Notify           (const QVariantMap &Notification) = 0;

  protected:
    explicit TorcNotifier(const QVariantMap &Details);
    virtual ~TorcNotifier() = default;
};

class TorcNotifierFactory
{
  public:
    TorcNotifierFactory();
    virtual ~TorcNotifierFactory() = default;

    static TorcNotifierFactory*   GetTorcNotifierFactory (void);
    TorcNotifierFactory*          NextFactory            (void) const;
    virtual TorcNotifier*         Create                 (const QString &Type, const QVariantMap &Details) = 0;

  protected:
    static TorcNotifierFactory*   gTorcNotifierFactory;
    TorcNotifierFactory*          nextTorcNotifierFactory;

  private:
    Q_DISABLE_COPY(TorcNotifierFactory)
};
#endif // TORCNOTIFIER_H
