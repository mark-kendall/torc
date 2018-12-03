/* Class TorcDB
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012-18
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QtSql>
#include <QThread>
#include <QHash>

// Torc
#include "torclogging.h"
#include "torcdb.h"

/*! \class TorcDB
 *  \brief Base Sql database access class.
 *
 * TorcDB is the base implementation for Sql database access. It is currently limited
 * to setting and retrieving settings and whilst designed for generic Sql
 * usage, the only current concrete subclass is TorcSQliteDB. Changes may (will) be required for
 * other database types and usage.
 *
 * By default, QSql database access is not thread safe and a new connection must be opened for
 * each thread that wishes to use the database. TorcDB (and its subclasses) will ask
 * for a connection each time the database is accessed and connections are cached on a per-thread
 * basis. The thread name (and pointer) is used for cache tracking and thus any thread must have
 * a name. This implicitly limits database access to a QThread or one its subclasses (e.g. TorcQThread) and
 * hence database access will not work from QRunnable's or other miscellaneous threads - this is by design
 * and is NOT a bug.
 *
 * \sa TorcSQLiteDB
 * \sa TorcLocalContext
*/
TorcDB::TorcDB(const QString &DatabaseName, const QString &DatabaseType)
  : m_databaseValid(false),
    m_databaseName(DatabaseName),
    m_databaseType(DatabaseType),
    m_lock(QMutex::Recursive),
    m_connectionMap()
{
}

TorcDB::~TorcDB()
{
    QMutexLocker locker(&m_lock);
    m_databaseValid = false;
    CloseConnections();
}

/*! \fn    TorcDB::IsValid
 *  \brief Returns true if the datbase has been opened/created.
*/
bool TorcDB::IsValid(void)
{
    QMutexLocker locker(&m_lock);
    return m_databaseValid;
}

/*! \fn    TorcDB::CloseConnections
 *  \brief Close all cached database connections.
*/
void TorcDB::CloseConnections(void)
{
    QMutexLocker locker(&m_lock);

    CloseThreadConnection();

    if (m_connectionMap.isEmpty())
        return;

    LOG(VB_GENERAL, LOG_WARNING, QString("%1 open connections.").arg(m_connectionMap.size()));

    QHashIterator<QThread*,QString> it(m_connectionMap);
    while (it.hasNext())
    {
        it.next();
        QString name = it.value();
        LOG(VB_GENERAL, LOG_INFO, QString("Removing connection '%1'").arg(name));
        QSqlDatabase::removeDatabase(name);
    }
    m_connectionMap.clear();
}

/*! \fn    TorcDB::CloseThreadConnection
 *  \brief Close the database connection for the current thread.
*/
void TorcDB::CloseThreadConnection(void)
{
    QThread* thread = QThread::currentThread();
    QMutexLocker locker(&m_lock);
    if (m_connectionMap.contains(thread))
    {
        QString name = m_connectionMap.value(thread);
        LOG(VB_GENERAL, LOG_INFO, QString("Removing connection '%1'").arg(name));
        QSqlDatabase::removeDatabase(name);
        m_connectionMap.remove(thread);
    }
}

/*! \fn    TorcDB::GetThreadConnection
 *  \brief Retrieve a database connection for the current thread.
 *
 * Database connections are cached (thus limiting connections to one per thread).
 *
 * \sa CloseConnections
 * \sa CloseThreadConnection
*/
QString TorcDB::GetThreadConnection(void)
{
    QThread* thread = QThread::currentThread();

    {
        QMutexLocker locker(&m_lock);
        if (m_connectionMap.contains(thread))
            return m_connectionMap.value(thread);
    }

    if (thread->objectName().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Database access is only available from TorcQThread");
        return QString();
    }

    QString name = QString("%1-%2").arg(thread->objectName(), QString::number((unsigned long long)thread));
    QSqlDatabase newdb = QSqlDatabase::addDatabase(m_databaseType, name);

    if (m_databaseType == "QSQLITE")
        newdb.setConnectOptions("QSQLITE_BUSY_TIMEOUT=1");
    newdb.setDatabaseName(m_databaseName);

    {
        QMutexLocker locker(&m_lock);
        m_connectionMap.insert(thread, name);
        LOG(VB_GENERAL, LOG_INFO, QString("New connection '%1'").arg(name));
    }

    return name;
}

/*! \fn    TorcDB::DebugError(QSqlQuery*)
 *  \brief Log database errors following a failed query.
 *
 * \sa Debugerror(QSqlDatabase*)
*/
bool TorcDB::DebugError(QSqlQuery *Query)
{
    if (!Query)
        return true;

    QSqlError error = Query->lastError();

    if (error.type() == QSqlError::NoError)
        return false;

    if (!error.databaseText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Database Error: %1")
            .arg(error.databaseText()));
        return true;
    }

    if (!error.driverText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Driver Error: %1")
            .arg(error.driverText()));
    }

    return true;
}

/*! \fn    TorcDB::DebugError(QSqlDatabase*)
 *  \brief Log database errors following database creation.
 *
 * \sa Debugerror(QSqlQuery*)
*/
bool TorcDB::DebugError(QSqlDatabase *Database)
{
    if (!Database)
        return true;

    QSqlError error = Database->lastError();

    if (error.type() == QSqlError::NoError)
        return false;

    if (!error.databaseText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Database Error: %1")
            .arg(error.databaseText()));
        return true;
    }

    if (!error.driverText().isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Driver Error: %1")
            .arg(error.driverText()));
    }

    return true;
}

/*! \fn    TorcDB::LoadSettings
 *  \brief Retrieve all persistent settings stored in the database.
 *
 * Settings are key/value pairs (of strings).
 *
 * Settings are designed as application wide configurable settings that can
 * only changed by the administrative user (if present, otherwise the default user).
 *
 * \sa SetSetting
*/
void TorcDB::LoadSettings(QMap<QString, QString> &Settings)
{
    QMutexLocker locker(&m_lock);
    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid() || !db.isOpen())
        return;

    QSqlQuery query("SELECT name, value from settings;", db);
    DebugError(&query);

    while (query.next())
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("'%1' : '%2'").arg(query.value(0).toString(), query.value(1).toString()));
        Settings.insert(query.value(0).toString(), query.value(1).toString());
    }
}

/*! \fn    TorcDB::SetSetting
 *  \brief Set the setting Name to the value Value.
*/
void TorcDB::SetSetting(const QString &Name, const QString &Value)
{
    QMutexLocker locker(&m_lock);
    if (Name.isEmpty())
        return;

    QSqlDatabase db = QSqlDatabase::database(GetThreadConnection());
    DebugError(&db);
    if (!db.isValid() || !db.isOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open database connection.");
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM settings where name=:NAME;");
    query.bindValue(":NAME", Name);
    query.exec();
    DebugError(&query);

    query.prepare("INSERT INTO settings (name, value) "
                  "VALUES (:NAME, :VALUE);");
    query.bindValue(":NAME", Name);
    query.bindValue(":VALUE", Value);
    query.exec();
    DebugError(&query);
}
