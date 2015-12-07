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
    TorcControl(const QVariantMap &Details);
    virtual ~TorcControl();

  public:
    virtual bool           Validate               (void);
    virtual TorcControl::Type GetType             (void) = 0;
    virtual void           Start                  (void);

  public slots:
    void                   InputValueChanged      (double Value);
    void                   InputValidChanged      (bool   Valid);

  protected:
    void                   Finish                 (bool Passthrough);
    void                   InputValidChangedPriv  (QObject* Input, bool Valid);
    void                   CheckInputValues       (void);
    void                   SetValue               (double Value);
    void                   SetValid               (bool Valid);
    virtual void           CalculateOutput        (void) = 0;
    bool                   CheckForCircularReferences (const QString &UniqueId, QString Path);

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
