#ifndef TORCOUTPUTS_H
#define TORCOUTPUTS_H

// Qt
#include <QList>
#include <QMutex>

// Torc
#include "torchttpservice.h"
#include "torcoutput.h"
#include "torccentral.h"

class TorcOutputs final : public QObject, public TorcHTTPService, public TorcDeviceHandler
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetOutputList",  "type=outputs")
    Q_PROPERTY(QVariantMap outputList  READ GetOutputList() NOTIFY OutputsChanged())
    Q_PROPERTY(QStringList outputTypes READ GetOutputTypes() CONSTANT)

  public:
    static TorcOutputs* gOutputs;

  public:
    TorcOutputs();

    void                Graph                     (QByteArray* Data);
    QString             GetUIName                 (void) override;

    // TorcDeviceHandler
    void                Create                    (const QVariantMap &Details) override;
    void                Destroy                   (void) override;

  public slots:
    // TorcHTTPService
    void                SubscriberDeleted         (QObject *Subscriber);

    QVariantMap         GetOutputList             (void);
    QStringList         GetOutputTypes            (void);

  signals:
    void                OutputsChanged            (void);

  public:
    void                AddOutput                 (TorcOutput *Output);
    void                RemoveOutput              (TorcOutput *Output);

  private:
    QList<TorcOutput*>  outputList;
    QStringList         outputTypes;
    QMap<QString,TorcOutput*>   m_createdOutputs;

  private:
    Q_DISABLE_COPY(TorcOutputs)
};

#endif // TORCOUTPUTS_H
