#ifndef TORCSSDP_H
#define TORCSSDP_H

// Qt
#include <QObject>

// Torc

#include "torcqthread.h"
#include "upnp/torcupnp.h"

class TorcSSDPPriv;

class TorcSSDP : public QObject
{
    friend class TorcSSDPThread;

    Q_OBJECT

  public:
    static bool      Search             (const QString &Type, QObject *Owner);
    static bool      CancelSearch       (const QString &Type, QObject *Owner);
    static bool      Announce           (const TorcUPNPDescription &Description);
    static bool      CancelAnnounce     (const TorcUPNPDescription &Description);

  protected:
    TorcSSDP();
    ~TorcSSDP();

    static TorcSSDP* Create             (bool Destroy = false);

  protected slots:
    void             SearchPriv         (const QString &Type, QObject *Owner);
    void             CancelSearchPriv   (const QString &Type, QObject *Owner);
    void             AnnouncePriv       (const TorcUPNPDescription Description);
    void             CancelAnnouncePriv (const TorcUPNPDescription Description);
    bool             event              (QEvent *Event);
    void             Read               (void);

  private:
    TorcSSDPPriv    *m_priv;
    int              m_searchTimer;
    int              m_refreshTimer;
};

class TorcSSDPThread : public TorcQThread
{
    Q_OBJECT

  public:
    TorcSSDPThread();

    void Start(void);
    void Finish(void);
};
#endif // TORCSSDP_H
