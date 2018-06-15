#ifndef TORCSYSTEMNOTIFICATION_H
#define TORCSYSTEMNOTIFICATION_H

// Torc
#include "torcnotification.h"

class TorcSystemNotification Q_DECL_FINAL : public TorcNotification
{
    Q_OBJECT

  public:
    explicit TorcSystemNotification(const QVariantMap &Details);
    ~TorcSystemNotification();

    QStringList GetDescription (void) Q_DECL_OVERRIDE;
    void        Graph          (QByteArray *Data) Q_DECL_OVERRIDE;

  public slots:
    // QObject
    bool         event (QEvent *Event) Q_DECL_OVERRIDE;

  private:
    QStringList  m_events;
};

#endif // TORCSYSTEMNOTIFICATION_H
