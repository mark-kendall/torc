#ifndef TORCSETTING_H
#define TORCSETTING_H

// Qt
#include <QSet>
#include <QMutex>
#include <QObject>
#include <QVariant>
#include <QStringList>

// Torc
#include "torchttpservice.h"
#include "torcreferencecounted.h"

class TorcSetting : public QObject, public TorcHTTPService, public TorcReferenceCounter
{

    Q_ENUMS(Type)

  public:
    enum Type
    {
        Bool,
        Integer,
        String,
        StringList
    };

  public:
    static QString TypeToString(Type type);
    TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName,
                Type SettingType, bool Persistent, const QVariant &Default);

  public:
    Q_OBJECT
    Q_CLASSINFO("Version",   "1.0.0")
    Q_PROPERTY (QVariant value        READ GetValue()        NOTIFY ValueChanged()       )
    Q_PROPERTY (QString  uiName       READ GetUiName()       CONSTANT                    )
    Q_PROPERTY (QString  description  READ GetDescription()  CONSTANT                    )
    Q_PROPERTY (QString  helpText     READ GetHelpText()     CONSTANT                    )
    Q_PROPERTY (QVariant defaultValue READ GetDefaultValue() CONSTANT                    )
    Q_PROPERTY (bool     persistent   READ GetPersistent()   CONSTANT                    )
    Q_PROPERTY (bool     isActive     READ GetIsActive()     NOTIFY ActiveChanged()      )
    Q_PROPERTY (QString  settingType  READ GetSettingType()  CONSTANT                    )

  public:
    QVariant::Type         GetStorageType       (void);
    void                   AddChild             (TorcSetting *Child);
    void                   RemoveChild          (TorcSetting *Child);
    void                   Remove               (void);
    TorcSetting*           FindChild            (const QString &Child, bool Recursive = false);
    QSet<TorcSetting*>     GetChildren          (void);
    TorcSetting*           GetChildByIndex      (int Index);
    void                   SetActiveThreshold   (int  Threshold);
    void                   SetDescription       (const QString &Description);
    void                   SetHelpText          (const QString &HelpText);
    void                   SetRange             (int Begin, int End, int Step);
    Type                   GetType              (void);

  public slots:
    void                   SubscriberDeleted    (QObject *Subscriber);
    void                   SetValue             (const QVariant &Value);
    bool                   GetIsActive          (void);
    void                   SetActive            (bool Value);
    QVariant               GetValue             (void);
    QString                GetUiName            (void);
    QString                GetDescription       (void);
    QString                GetHelpText          (void);
    QVariant               GetDefaultValue      (void);
    bool                   GetPersistent        (void);
    QString                GetSettingType       (void);

    // Checkbox
    void                   SetTrue              (void);
    void                   SetFalse             (void);

    // Integer
    int                    GetBegin             (void);
    int                    GetEnd               (void);
    int                    GetStep              (void);

  signals:
    void                   ValueChanged         (const QVariant &Value);
    void                   ValueChanged         (int  Value);
    void                   ValueChanged         (bool Value);
    void                   ValueChanged         (QString Value);
    void                   ValueChanged         (QStringList Value);
    void                   ActiveChanged        (bool Active);
    void                   Removed              (void);

  protected:
    virtual               ~TorcSetting();

  private:
    Q_DISABLE_COPY(TorcSetting);

  private:
    TorcSetting           *m_parent;
    Type                   type;
    QString                settingType;
    bool                   persistent;
    QString                m_dbName;
    QString                uiName;
    QString                description;
    QString                helpText;
    QVariant               value;
    QVariant               defaultValue;

    // Integer
    int                    m_begin;
    int                    m_end;
    int                    m_step;

  private:
    bool                   isActive;
    int                    m_active;
    int                    m_activeThreshold;
    QList<TorcSetting*>    m_children;
    QMutex                *m_childrenLock;
};

class TorcSettingGroup : public TorcSetting
{
  public:
    TorcSettingGroup(TorcSetting *Parent, const QString &UIName);
};

Q_DECLARE_METATYPE(TorcSetting*);

#endif // TORCSETTING_H
