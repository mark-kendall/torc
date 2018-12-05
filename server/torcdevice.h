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

#define DEVICE_LINE_ITEM QStringLiteral("|<FONT POINT-SIZE=\"10\">%1</FONT>")
#define DEVICE_CONSTANT QStringLiteral("Constant")

class TorcDevice : public QObject, public TorcReferenceCounter
{
    friend class TorcCentral;

    Q_OBJECT
    Q_PROPERTY(bool     valid                   READ GetValid()                   NOTIFY   ValidChanged(Valid))
    Q_PROPERTY(double   value                   READ GetValue()                   NOTIFY   ValueChanged(Value))
    Q_PROPERTY(QString  modelId                 READ GetModelId()                 CONSTANT)
    Q_PROPERTY(QString  uniqueId                READ GetUniqueId()                CONSTANT)
    Q_PROPERTY(QString  userName                READ GetUserName()                CONSTANT)
    Q_PROPERTY(QString  userDescription         READ GetUserDescription()         CONSTANT)

  public:
    TorcDevice(bool Valid, double Value, double Default,
               const QString &ModelId,   const QVariantMap &Details);

    virtual void           Start                  (void);
    virtual void           Stop                   (void);
    virtual QStringList    GetDescription         (void);

  public slots:
    virtual void           SetValue               (double Value);
    virtual void           SetValid               (bool   Valid);

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

  protected:
    virtual ~TorcDevice();

  protected:
    bool                   valid;
    double                 value;
    double                 defaultValue;
    QString                modelId;
    QString                uniqueId;
    QString                userName;
    QString                userDescription;
    QMutex                 lock;

    static QHash<QString,TorcDevice*> *gDeviceList;
    static QMutex         *gDeviceListLock;

    bool                   wasInvalid;
};

#endif // TORCDEVICE_H
