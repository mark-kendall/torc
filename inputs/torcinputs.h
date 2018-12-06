#ifndef TORCINPUTS_H
#define TORCINPUTS_H

// Qt
#include <QList>
#include <QMutex>

// Torc
#include "torchttpservice.h"
#include "torcinput.h"
#include "torccentral.h"

class TorcInputs final : public QObject, public TorcHTTPService, public TorcDeviceHandler
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetInputList",   "type=inputs")
    Q_PROPERTY(QVariantMap inputList   READ GetInputList() NOTIFY InputsChanged())
    Q_PROPERTY(QStringList inputTypes  READ GetInputTypes() CONSTANT)

  public:
    static TorcInputs* gInputs;

  public:
    TorcInputs();
    ~TorcInputs() = default;

    void                Graph                    (QByteArray* Data);
    QString             GetUIName                (void) override;

    // TorcDeviceHandler
    void                Create                   (const QVariantMap &Details) override;
    void                Destroy                  (void) override;

  public slots:
    // TorcHTTPService
    void                SubscriberDeleted        (QObject *Subscriber);

    QVariantMap         GetInputList             (void);
    QStringList         GetInputTypes            (void);

  signals:
    void                InputsChanged            (void);

  public:
    void                AddInput                 (TorcInput *Input);
    void                RemoveInput              (TorcInput *Input);

  private:
    QList<TorcInput*>   inputList;
    QStringList         inputTypes;
    QMap<QString,TorcInput*> m_createdInputs;
};

#endif // TORCINPUTS_H
