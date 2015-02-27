#ifndef TORCCONTROL_H
#define TORCCONTROL_H

// Qt
#include <QMap>
#include <QObject>
#include <QStringList>

// Torc
#include "torcdevice.h"

class TorcControl : public TorcDevice
{
    friend class TorcControls;

    Q_OBJECT
    Q_ENUMS(Operation)
    Q_PROPERTY(bool    valid           READ GetValid()           NOTIFY ValidChanged())
    Q_PROPERTY(double  value           READ GetValue()           NOTIFY ValueChanged())
    Q_PROPERTY(QString uniqueId        READ GetUniqueId()        CONSTANT)
    Q_PROPERTY(QString userName        READ GetUserName()        WRITE SetUserName()        NOTIFY UserNameChanged())
    Q_PROPERTY(QString userDescription READ GetUserDescription() WRITE SetUserDescription() NOTIFY UserDescriptionChanged())

  public:
    enum Type
    {
        UnknownType,
        Logic,
        Timer,
        Transition
    };

    static TorcControl::Type      StringToType      (const QString &Type);
    static QString                TypeToString      (TorcControl::Type Type);
    static bool                   ParseTimeString   (const QString &Time, int &Days, int &Hours,
                                                     int &Minutes, int &Seconds, quint64 &DurationInSeconds);

  protected:
    TorcControl(const QString &UniqueId, const QVariantMap &Details);
    virtual ~TorcControl();

  public:
    virtual bool           Validate               (void);
    virtual TorcControl::Type GetType             (void) = 0;
    virtual void           Start                  (void);

  public slots:
    void                   InputValueChanged      (double Value);
    void                   InputValidChanged      (bool   Valid);

    bool                   GetValid               (void);
    double                 GetValue               (void);
    QString                GetUniqueId            (void);
    QString                GetUserName            (void);
    QString                GetUserDescription     (void);

    void                   SetUserName            (const QString &Name);
    void                   SetUserDescription     (const QString &Description);

  signals:
    void                   ValidChanged           (bool  Valid);
    void                   ValueChanged           (double Value);
    void                   UserNameChanged        (const QString &Name);
    void                   UserDescriptionChanged (const QString &Description);

  protected:
    void                   Finish                 (bool Passthrough);
    void                   InputValidChangedPriv  (QObject* Input, bool Valid);
    void                   CheckInputValues       (void);
    void                   SetValue               (double Value);
    void                   SetValid               (bool Valid);
    virtual void           CalculateOutput        (void) = 0;

  protected:
    bool                   m_parsed;
    bool                   m_validated;
    QStringList            m_inputList;
    QStringList            m_outputList;
    QMap<QObject*,QString> m_inputs;
    QMap<QObject*,QString> m_outputs;
    QMap<QObject*,double>  m_inputValues;
    QMap<QObject*,bool>    m_inputValids;
    bool                   m_allInputsValid;
};

#endif // TORCCONTROL_H
