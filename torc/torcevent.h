#ifndef TORCEVENT_H
#define TORCEVENT_H

// Qt
#include <QMap>
#include <QVariant>
#include <QEvent>

class TorcEvent : public QEvent
{
  public:
    TorcEvent(int Event, const QVariantMap &Data = QVariantMap());
    virtual ~TorcEvent() = default;

    int          GetEvent (void);
    QVariantMap& Data     (void);
    TorcEvent*   Copy     (void) const;

    static       Type      TorcEventType;

  private:
    Q_DISABLE_COPY(TorcEvent)
    int         m_event;
    QVariantMap m_data;
};

#endif // TORCEVENT_H
