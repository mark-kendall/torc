#ifndef TORCDEVICEHANDLER_H
#define TORCDEVICEHANDLER_H

// Qt
#include <QHash>
#include <QMutex>
#include <QReadWriteLock>
#include <QVariant>

class TorcDeviceHandler
{
  public:
    TorcDeviceHandler();
    virtual ~TorcDeviceHandler();

    static  void Start   (const QVariantMap &Details);
    static  void Stop    (void);

  protected:
    virtual void                     Create           (const QVariantMap &Details) = 0;
    virtual void                     Destroy          (void) = 0;
    static TorcDeviceHandler*        GetDeviceHandler (void);
    TorcDeviceHandler*               GetNextHandler   (void);

  protected:
    QReadWriteLock                   m_handlerLock;

  private:
    static QList<TorcDeviceHandler*> gTorcDeviceHandlers;
    static TorcDeviceHandler        *gTorcDeviceHandler;
    static QMutex                   *gTorcDeviceHandlersLock;
    TorcDeviceHandler               *m_nextTorcDeviceHandler;

  private:
    Q_DISABLE_COPY(TorcDeviceHandler)
};

#endif // TORCDEVICEHANDLER_H
