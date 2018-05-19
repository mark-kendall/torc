#ifndef TORCUSER_H
#define TORCUSER_H

// Qt
#include <QObject>

// Torc
#include "torchttpservice.h"

class TorcUser : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",     "1.0.0")
    Q_CLASSINFO("RestartTorc", "methods=PUT")
    Q_CLASSINFO("StopTorc",    "methods=PUT")
    Q_PROPERTY(bool         canRestartTorc     READ GetCanRestartTorc CONSTANT)
    Q_PROPERTY(bool         canStopTorc        READ GetCanStopTorc    CONSTANT)

  public:
    TorcUser();
   ~TorcUser();

  public slots:
    // TorcHTTPService
    void           SubscriberDeleted     (QObject *Subscriber);

    bool           GetCanRestartTorc    (void);
    void           RestartTorc          (void);
    void           StopTorc             (void);
    bool           GetCanStopTorc       (void);

  private:
    bool           canRestartTorc;
    bool           canStopTorc;
    QMutex         m_lock;
};

#endif // TORCUSER_H
