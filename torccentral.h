#ifndef TORCCENTRAL_H
#define TORCCENTRAL_H

// Qt
#include <QMutex>
#include <QObject>
#include <QVariant>

// Torc
#include "torcdevice.h"
#include "http/torchttpservice.h"

class TorcCentral : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",     "1.0.0")
    Q_CLASSINFO("RestartTorc", "methods=PUT")
    Q_PROPERTY(bool canRestartTorc READ GetCanRestartTorc NOTIFY CanRestartTorcChanged)

  public:
    TorcCentral();
    ~TorcCentral();

    QString         GetUIName   (void);
    bool            event       (QEvent *Event);

  signals:
    void            CanRestartTorcChanged (bool CanRestartTorc);

  public slots:
    // TorcHTTPService
    void SubscriberDeleted      (QObject *Subscriber);

    bool            GetCanRestartTorc (void);
    bool            RestartTorc (void);

  private:
    bool            LoadConfig  (void);

  private:
    QMutex         *m_lock;
    QVariantMap     m_config;
    QByteArray     *m_graph;
    bool            canRestartTorc;
};
#endif // TORCCENTRAL_H
