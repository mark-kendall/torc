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
        Switch,
        PWM,
        MaxType
     };

    static QString          TypeToString(TorcOutput::Type Type);
    static TorcOutput::Type StringToType(const QString &Type);

  public:
    TorcOutput(TorcOutput::Type Type, double Value, const QString &ModelId, const QVariantMap &Details);
    virtual ~TorcOutput();

    virtual TorcOutput::Type GetType (void) = 0;

    bool             HasOwner               (void);
    bool             SetOwner               (QObject *Owner);

  public slots:
    // TorcHTTPService
    void             SubscriberDeleted         (QObject *Subscriber);

  private:
    QObject         *m_owner;
};

#endif // TORCOUTPUT_H
