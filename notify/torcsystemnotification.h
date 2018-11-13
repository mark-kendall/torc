#ifndef TORCSYSTEMNOTIFICATION_H
#define TORCSYSTEMNOTIFICATION_H

// Torc
#include "torclocalcontext.h"
#include "torcnotification.h"

class TorcSystemNotification final : public TorcNotification
{
    Q_OBJECT

  public:
    explicit TorcSystemNotification(const QVariantMap &Details);
    ~TorcSystemNotification();

    QStringList GetDescription (void) override;
    void        Graph          (QByteArray *Data) override;

  public slots:
    // QObject
    bool        event (QEvent *Event) override;

  private:
    QList<Torc::Actions>  m_events;
};

#endif // TORCSYSTEMNOTIFICATION_H
