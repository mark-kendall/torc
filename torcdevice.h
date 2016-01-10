#ifndef TORCDEVICE_H
#define TORCDEVICE_H

// Qt
#include <QHash>
#include <QMutex>
#include <QString>
#include <QObject>

// Torc
#include "torcreferencecounted.h"
#include "torcdevicehandler.h"

#define DEVICE_LINE_ITEM "|<FONT POINT-SIZE=\"10\">%1</FONT>"

class TorcDevice : public QObject, public TorcReferenceCounter
{
    friend class TorcCentral;

    Q_OBJECT
    Q_PROPERTY(bool     valid                   READ GetValid()                   NOTIFY   ValidChanged())
    Q_PROPERTY(double   value                   READ GetValue()                   NOTIFY   ValueChanged())
    Q_PROPERTY(QString  modelId                 READ GetModelId()                 CONSTANT)
    Q_PROPERTY(QString  uniqueId                READ GetUniqueId()                CONSTANT)
    Q_PROPERTY(QString  userName                READ GetUserName()                WRITE    SetUserName()        NOTIFY UserNameChanged())
    Q_PROPERTY(QString  userDescription         READ GetUserDescription()         WRITE    SetUserDescription() NOTIFY UserDescriptionChanged())

  public:
    TorcDevice(bool Valid, double Value, double Default,
               const QString &ModelId,   const QVariantMap &Details);
    virtual ~TorcDevice();

    virtual void           Start                  (void);
    virtual void           Stop                   (void);
    virtual QStringList    GetDescription         (void);

  public slots:
    virtual void           SetValue               (double Value);
    virtual void           SetValid               (bool   Valid);
    void                   SetUserName            (const QString &Name);
    void                   SetUserDescription     (const QString &Description);

    bool                   GetValid               (void);
    double                 GetValue               (void);
    double                 GetDefaultValue        (void);
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

    static QHash<QString,TorcDevice*> *gDeviceList;
    static QMutex         *gDeviceListLock;
};

#endif // TORCDEVICE_H
