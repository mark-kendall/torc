#ifndef TORCCONTROL_H
#define TORCCONTROL_H

// Qt
#include <QMap>
#include <QObject>
#include <QStringList>

// Torc
#include "torchttpservice.h"
#include "torcdevice.h"

#define CONTROLS_DIRECTORY QString("controls")

class TorcControl : public TorcDevice, public TorcHTTPService
{
    friend class TorcControls;

    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")

  public:
    enum Type
    {
        Logic,
        Timer,
        Transition
    };

    Q_ENUM(Type)

    static bool                   ParseTimeString   (const QString &Time, int &Days, int &Hours,
                                                     int &Minutes, int &Seconds, quint64 &DurationInSeconds);
    static QString                DurationToString  (int Days, quint64 Duration);

  protected:
    TorcControl(TorcControl::Type Type, const QVariantMap &Details);
    virtual ~TorcControl() = default;

  public:
    virtual bool           Validate               (void);
    virtual TorcControl::Type GetType             (void) const = 0;
    virtual bool           IsPassthrough          (void);
    virtual bool           AllowInputs            (void) const;
    bool                   IsKnownInput           (const QString &Input) const;
    bool                   IsKnownOutput          (const QString &Output) const;
    QString                GetUIName              (void) override;

  public slots:
    // TorcHTTPService
    void                   SubscriberDeleted      (QObject *Subscriber);

    void                   InputValueChanged      (double Value);
    void                   InputValidChanged      (bool   Valid);

  protected:
    void                   Graph                  (QByteArray* Data);
    bool                   Finish                 (void);
    void                   InputValidChangedPriv  (QObject* Input, bool Valid);
    void                   CheckInputValues       (void);
    void                   SetValue               (double Value) override;
    void                   SetValid               (bool Valid) override;
    virtual void           CalculateOutput        (void) = 0;
    bool                   CheckForCircularReferences (const QString &UniqueId, const QString &Path) const;

  protected:
    bool                   m_parsed;
    bool                   m_validated;
    QStringList            m_inputList;
    QStringList            m_outputList;
    QMap<QObject*,QString> m_inputs;
    QMap<QObject*,QString> m_outputs;
    QMap<QObject*,double>  m_inputValues;
    QMap<QObject*,double>  m_lastInputValues;
    QMap<QObject*,bool>    m_inputValids;
    bool                   m_allInputsValid;
};

#endif // TORCCONTROL_H
