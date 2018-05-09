#ifndef TORCDB_H
#define TORCDB_H

// Qt
#include <QObject>
#include <QMutex>

class QSqlDatabase;
class QSqlQuery;

class TorcDB
{
  public:
    TorcDB(const QString &DatabaseName, const QString &DatabaseType);
    virtual ~TorcDB();

    static bool  DebugError             (QSqlQuery *Query);
    static bool  DebugError             (QSqlDatabase *Database);
    bool         IsValid                (void) const;
    void         CloseThreadConnection  (void);

    void         LoadSettings           (QMap<QString,QString> &Settings);
    void         LoadPreferences        (QMap<QString,QString> &Preferences);
    void         SetSetting             (const QString &Name, const QString &Value);
    void         SetPreference          (const QString &Name, const QString &Value);

  protected:
    virtual bool InitDatabase           (void) = 0;
    QString      GetThreadConnection    (void);
    void         CloseConnections       (void);

  protected:
    bool         m_databaseValid;
    QString      m_databaseName;
    QString      m_databaseType;
    QMutex       m_lock;
    QHash<QThread*,QString> m_connectionMap;
};

#endif // TORCDB_H
