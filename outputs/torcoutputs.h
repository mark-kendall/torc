#ifndef TORCOUTPUTS_H
#define TORCOUTPUTS_H

// Qt
#include <QList>
#include <QMutex>

// Torc
#include "torchttpservice.h"
#include "torcoutput.h"
#include "torccentral.h"

class TorcOutputs Q_DECL_FINAL : public QObject, public TorcHTTPService
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
    ~TorcOutputs();

    void                Graph                     (QByteArray* Data);
    QString             GetUIName                 (void) Q_DECL_OVERRIDE;

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
    QMutex              m_lock;

  private:
    Q_DISABLE_COPY(TorcOutputs)
};

#endif // TORCOUTPUTS_H
