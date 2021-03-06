#ifndef TORCADMINTHREAD_H
#define TORCADMINTHREAD_H

// Qt
#include <QList>

// Torc
#include "torcqthread.h"

class QMutex;

class TorcAdminThread : public TorcQThread
{
    Q_OBJECT
  public:
    TorcAdminThread();
    virtual ~TorcAdminThread() = default;

    void Start  (void);
    void Finish (void);
};

#define TORC_ADMIN_CRIT_PRIORITY 200
#define TORC_ADMIN_HIGH_PRIORITY 100
#define TORC_ADMIN_MED_PRIORITY   50
#define TORC_ADMIN_LOW_PRIORITY   10

class TorcAdminObject
{
  public:
    static void CreateObjects      (void);
    static void DestroyObjects     (void);
    static bool HigherPriority     (const TorcAdminObject *Object1,
                                    const TorcAdminObject *Object2);

  public:
    explicit TorcAdminObject(int Priority = TORC_ADMIN_LOW_PRIORITY);
    virtual ~TorcAdminObject() = default;

    int              Priority      (void) const;
    TorcAdminObject* GetNextObject (void);
    virtual void     Create        (void) = 0;
    virtual void     Destroy       (void) = 0;

  private:
    static QList<TorcAdminObject*> gTorcAdminObjects;
    static TorcAdminObject        *gTorcAdminObject;
    static QMutex                 *gTorcAdminObjectsLock;
    TorcAdminObject               *m_nextTorcAdminObject;
    int                            m_priority;

  private:
    Q_DISABLE_COPY(TorcAdminObject)
};

#endif // TORCADMINTHREAD_H
