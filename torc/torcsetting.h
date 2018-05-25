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
    Q_ENUMS(Role)

  public:
    enum Type
    {
        Bool,
        Integer,
        String,
        StringList,
        Group
    };

    enum Role
    {
        None       = (0 << 0),
        Persistent = (1 << 0),
        Public     = (1 << 1)
    };

    Q_DECLARE_FLAGS(Roles, Role)

  public:
    static QString TypeToString(Type type);
    TorcSetting(TorcSetting *Parent, const QString &DBName, const QString &UIName,
                Type SettingType, Roles SettingRoles, const QVariant &Default);

  public:
    Q_OBJECT
    Q_CLASSINFO("Version", "1.0.0")
    Q_CLASSINFO("Secure",  "")
    Q_CLASSINFO("GetChildList", "type=settings,methods=AUTH")
    Q_PROPERTY (QVariant    value        READ GetValue        NOTIFY ValueChanged  )
    Q_PROPERTY (QString     uiName       READ GetUiName       CONSTANT             )
    Q_PROPERTY (QString     helpText     READ GetHelpText     CONSTANT             )
    Q_PROPERTY (QVariant    defaultValue READ GetDefaultValue CONSTANT             )
    Q_PROPERTY (bool        isActive     READ GetIsActive     NOTIFY ActiveChanged )
    Q_PROPERTY (QString     settingType  READ GetSettingType  CONSTANT             )
    Q_PROPERTY (QVariantMap selections   READ GetSelections   CONSTANT             )

  public:
    void                   Remove               (void);
    void                   SetActiveThreshold   (int  Threshold);
    void                   SetHelpText          (const QString &HelpText);
    void                   SetRange             (int Begin, int End, int Step);
    void                   SetSelections        (QVariantMap &Selections);

  public slots:
    void                   SubscriberDeleted    (QObject *Subscriber);
    QVariantMap            GetChildList         (void);
    bool                   SetValue             (const QVariant &Value);
    bool                   GetIsActive          (void);
    void                   SetActive            (bool Value);
    QVariant               GetValue             (void);
    QString                GetUiName            (void);
    QString                GetHelpText          (void);
    QVariant               GetDefaultValue      (void);
    QString                GetSettingType       (void);
    QVariantMap            GetSelections        (void);

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
    QString                GetChildList         (QMap<QString,QVariant> &Children);
    TorcSetting*           FindChild            (const QString &Child, bool Recursive = false);
    QSet<TorcSetting*>     GetChildren          (void);
    void                   AddChild             (TorcSetting *Child);
    void                   RemoveChild          (TorcSetting *Child);

  private:
    Q_DISABLE_COPY(TorcSetting)

  private:
    TorcSetting           *m_parent;
    Type                   type;
    QString                settingType;
    Roles                  roles;
    QString                m_dbName;
    QString                uiName;
    QString                helpText;
    QVariant               value;
    QVariant               defaultValue;
    QVariantMap            selections;

    // Integer
    int                    m_begin;
    int                    m_end;
    int                    m_step;

    bool                   isActive;
    int                    m_active;
    int                    m_activeThreshold;
    QList<TorcSetting*>    m_children;
    QMutex                 m_lock;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(TorcSetting::Roles)

class TorcSettingGroup : public TorcSetting
{
  public:
    TorcSettingGroup(TorcSetting *Parent, const QString &UIName);
};

Q_DECLARE_METATYPE(TorcSetting*)

#endif // TORCSETTING_H
