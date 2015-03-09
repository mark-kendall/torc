#ifndef TORCDEVICE_H
#define TORCDEVICE_H

// Qt
#include <QHash>
#include <QMutex>
#include <QString>
#include <QObject>

// Torc
#include "torcreferencecounted.h"

class TorcDevice : public QObject, public TorcReferenceCounter
{
    Q_OBJECT
    Q_PROPERTY(bool     valid                   READ GetValid()                   NOTIFY   ValidChanged())
    Q_PROPERTY(double   value                   READ GetValue()                   NOTIFY   ValueChanged())
    Q_PROPERTY(QString  modelId                 READ GetModelId()                 CONSTANT)
    Q_PROPERTY(QString  uniqueId                READ GetUniqueId()                CONSTANT)
    Q_PROPERTY(QString  userName                READ GetUserName()                WRITE    SetUserName()        NOTIFY UserNameChanged())
    Q_PROPERTY(QString  userDescription         READ GetUserDescription()         WRITE    SetUserDescription() NOTIFY UserDescriptionChanged())

  public:
    TorcDevice(bool Valid, double Value, double Default,
               const QString &ModelId, const QString &UniqueId,
               const QVariantMap &Details);
    virtual ~TorcDevice();

  public slots:
    virtual void           SetValue               (double Value);
    virtual void           SetValid               (bool   Valid);
    void                   SetUserName            (const QString &Name);
    void                   SetUserDescription     (const QString &Description);

    bool                   GetValid               (void);
    double                 GetValue               (void);
    QString                GetModelId             (void);
    QString                GetUniqueId            (void);
    QString                GetUserName            (void);
    QString                GetUserDescription     (void);

  signals:
    void                   ValidChanged           (bool   Valid);
    void                   ValueChanged           (double Value);
    void                   UserNameChanged        (const QString &Name);
    void                   UserDescriptionChanged (const QString &Description);

  protected:
    bool                   valid;
    double                 value;
    double                 defaultValue;
    QString                modelId;
    QString                uniqueId;
    QString                userName;
    QString                userDescription;
    QMutex                *lock;

  public:
    static QHash<QString,QObject*> *gDeviceList;
    static QMutex         *gDeviceListLock;
    static bool            UniqueIdAvailable  (const QString &UniqueId);
    static bool            RegisterUniqueId   (const QString &UniqueId, QObject *Object);
    static void            UnregisterUniqueId (const QString &UniqueId);
    static QObject*        GetObjectforId     (const QString &UniqueId);
};

class TorcDeviceHandler
{
  public:
    TorcDeviceHandler();
   ~TorcDeviceHandler();

    static  void Start   (const QVariantMap &Details);
    static  void Stop    (void);

  protected:
    virtual void                     Create           (const QVariantMap &Details) = 0;
    virtual void                     Destroy          (void) = 0;
    static TorcDeviceHandler*        GetDeviceHandler (void);
    TorcDeviceHandler*               GetNextHandler   (void);

  protected:
    QMutex                          *m_lock;

  private:
    static QList<TorcDeviceHandler*> gTorcDeviceHandlers;
    static TorcDeviceHandler        *gTorcDeviceHandler;
    static QMutex                   *gTorcDeviceHandlersLock;
    TorcDeviceHandler               *m_nextTorcDeviceHandler;
};
#endif // TORCDEVICE_H
