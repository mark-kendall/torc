#ifndef TORCOUTPUT_H
#define TORCOUTPUT_H

// Qt
#include <QMutex>
#include <QObject>

// Torc
#include "torcreferencecounted.h"
#include "torchttpservice.h"
#include "torcdevice.h"

#define OUTPUTS_DIRECTORY QString("outputs")

class TorcOutput : public TorcDevice, public TorcHTTPService
{
    Q_OBJECT
    Q_ENUMS(Type)
    Q_CLASSINFO("Version",        "1.0.0")

  public:
    enum Type
    {
        Unknown    = 0,
        Temperature,
        pH,
        Switch,
        PWM,
        Camera,
        MaxType
     };

    static QString          TypeToString(TorcOutput::Type Type);
    static TorcOutput::Type StringToType(const QString &Type);

  public:
    TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details);
    TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details,
               QObject *Output, const QMetaObject &MetaObject, const QString &Blacklist = QString(""));

    virtual ~TorcOutput();

    virtual TorcOutput::Type GetType (void) = 0;

    bool             HasOwner               (void);
    bool             SetOwner               (QObject *Owner);
    QString          GetUIName              (void) Q_DECL_OVERRIDE;

  public slots:
    // TorcHTTPService
    void             SubscriberDeleted         (QObject *Subscriber);

  private:
    QObject         *m_owner;

  private:
    Q_DISABLE_COPY(TorcOutput)
};

#endif // TORCOUTPUT_H
