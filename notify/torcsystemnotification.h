#ifndef TORCSYSTEMNOTIFICATION_H
#define TORCSYSTEMNOTIFICATION_H

// Torc
#include "torcnotification.h"

class TorcSystemNotification : public TorcNotification
{
    Q_OBJECT

  public:
    TorcSystemNotification(const QVariantMap &Details);
    ~TorcSystemNotification();

  public slots:
    // QObject
    bool         event (QEvent *Event);

  private:
    QStringList  m_events;
};

#endif // TORCSYSTEMNOTIFICATION_H