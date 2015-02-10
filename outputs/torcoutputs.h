#ifndef TORCOUTPUTS_H
#define TORCOUTPUTS_H

// Qt
#include <QList>
#include <QMutex>

// Torc
#include "torchttpservice.h"
#include "torcoutput.h"

class TorcOutputs : public QObject, public TorcHTTPService
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetOutputList",  "type=outputs")
    Q_PROPERTY(QVariantMap outputList READ GetOutputList() NOTIFY OutputsChanged())

  public:
    static TorcOutputs* gOutputs;

  public:
    TorcOutputs();
    ~TorcOutputs();

  public slots:
    // TorcHTTPService
    void                SubscriberDeleted         (QObject *Subscriber);

    QVariantMap         GetOutputList             (void);

  signals:
    void                OutputsChanged            (void);

  public:
    void                AddOutput                 (TorcOutput *Output);
    void                RemoveOutput              (TorcOutput *Output);

  private:
    QList<TorcOutput*>  outputList;
    QMutex             *m_lock;
};

#endif // TORCOUTPUTS_H
