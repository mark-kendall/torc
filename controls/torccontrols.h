#ifndef TORCCONTROLS_H
#define TORCCONTROLS_H


// Torc
#include "torchttpservice.h"
#include "torccentral.h"
#include "torccontrol.h"

class TorcControls : public QObject, public TorcHTTPService, public TorcDeviceHandler
{
    Q_OBJECT
    Q_CLASSINFO("Version",        "1.0.0")
    Q_CLASSINFO("GetControlList", "type=controls")
    Q_PROPERTY(QVariantMap controlList  READ GetControlList() NOTIFY ControlsChanged())
    Q_PROPERTY(QStringList controlTypes READ GetControlTypes() CONSTANT)

  public:
    TorcControls();

    static TorcControls* gControls;

    void                Create                    (const QVariantMap &Details) override;
    void                Destroy                   (void) override;
    void                Validate                  (void);
    void                Graph                     (QByteArray* Data);
    QString             GetUIName                 (void) override;

  public slots:
    // TorcHTTPService
    void                SubscriberDeleted         (QObject *Subscriber);

    QVariantMap         GetControlList            (void);
    QStringList         GetControlTypes           (void) const;

  signals:
    void                ControlsChanged           (void);

  private:
    QList<TorcControl*> controlList;
    QStringList         controlTypes;
};

#endif // TORCCONTROLS_H
