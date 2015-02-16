#ifndef TORCCENTRAL_H
#define TORCCENTRAL_H

// Qt
#include <QMutex>
#include <QObject>
#include <QVariant>

class TorcCentral : public QObject
{
    Q_OBJECT

  public:
    TorcCentral();
    ~TorcCentral();

    bool        event          (QEvent *Event);

    static      QByteArray    *gStateGraph;
    static      QMutex        *gStateGraphLock;

  public slots:
    void        SensorsChanged (void);
    void        OutputsChanged (void);

  private:
    bool        LoadConfig     (void);

  private:
    QVariantMap m_config;
};

class TorcDeviceHandler
{
  public:
    TorcDeviceHandler();
   ~TorcDeviceHandler();

    static  void Start   (const QVariantMap &Details);
    static  void Stop    (void);

  protected:
    virtual void                     Create  (const QVariantMap &Details) = 0;
    virtual void                     Destroy (void) = 0;
    static TorcDeviceHandler*        GetDeviceHandler (void);
    TorcDeviceHandler*               GetNextHandler (void);

  protected:
    QMutex                          *m_lock;

  private:
    static QList<TorcDeviceHandler*> gTorcDeviceHandlers;
    static TorcDeviceHandler        *gTorcDeviceHandler;
    static QMutex                   *gTorcDeviceHandlersLock;
    TorcDeviceHandler               *m_nextTorcDeviceHandler;
};

#endif // TORCCENTRAL_H
