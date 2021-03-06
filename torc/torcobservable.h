#ifndef TORCOBSERVABLE_H
#define TORCOBSERVABLE_H

// Torc
#include "torcevent.h"

class QObject;
class QMutex;

class TorcObservable
{
  public:
    TorcObservable();
    virtual ~TorcObservable() = default;

    void            AddObserver    (QObject* Observer);
    void            RemoveObserver (QObject* Observer);
    void            Notify         (const TorcEvent &Event);

  private:
    QMutex          m_observerLock;
    QList<QObject*> m_observers;

  private:
    Q_DISABLE_COPY(TorcObservable)
};

#endif // TORCOBSERVABLE_H
