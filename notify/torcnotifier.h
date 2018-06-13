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
    virtual bool event            (QEvent *e) Q_DECL_OVERRIDE;
    virtual void Notify           (const QVariantMap &Notification) = 0;
    virtual QStringList GetDescription (void) = 0;

  protected:
    explicit TorcNotifier(const QVariantMap &Details);
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

  private:
    Q_DISABLE_COPY(TorcNotifierFactory)
};
#endif // TORCNOTIFIER_H
