#include "DatabaseManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QCoreApplication>
#include <QRandomGenerator>

namespace {
const QString kConnectionName = QStringLiteral("account_db");

QByteArray randomBytes(int count)
{
    QByteArray bytes;
    bytes.reserve(count);
    while (bytes.size() < count)
    {
        const quint32 value = QRandomGenerator::system()->generate();
        bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    return bytes.left(count);
}
} // namespace

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
{
}

DatabaseManager::~DatabaseManager()
{
    closeConnection();
}

QString DatabaseManager::dataDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/data");
}

QStringList DatabaseManager::databaseFileNames() const
{
    QDir dir(dataDirectory());
    dir.setNameFilters(QStringList() << QStringLiteral("*.db"));
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);
    return dir.entryList();
}

QString DatabaseManager::createDatabase()
{
    if (!QDir().mkpath(dataDirectory()))
    {
        return QString();
    }

    // 随机生成 SHA256 文件名（64 位十六进制），冲突则重新生成
    QString fileName;
    while (true)
    {
        const QString candidate = QString::fromLatin1(
                                      QCryptographicHash::hash(randomBytes(32), QCryptographicHash::Sha256).toHex())
                                  + QStringLiteral(".db");
        if (!QFile::exists(dataDirectory() + QLatin1Char('/') + candidate))
        {
            fileName = candidate;
            break;
        }
    }

    closeConnection();
    if (!openConnection(fileName))
    {
        return QString();
    }

    QSqlDatabase database = QSqlDatabase::database(kConnectionName);
    if (!ensureSchema(database))
    {
        closeConnection();
        QFile::remove(dataDirectory() + QLatin1Char('/') + fileName);
        return QString();
    }

    m_CurrentFileName = fileName;
    return fileName;
}

bool DatabaseManager::deleteDatabase(const QString& fileName)
{
    if (fileName.isEmpty())
    {
        return false;
    }
    if (fileName == m_CurrentFileName)
    {
        closeConnection();
    }
    return QFile::remove(dataDirectory() + QLatin1Char('/') + fileName);
}

bool DatabaseManager::openDatabase(const QString& fileName)
{
    if (fileName.isEmpty())
    {
        closeConnection();
        return false;
    }
    if (fileName == m_CurrentFileName && isOpen())
    {
        return true;
    }
    if (!openConnection(fileName))
    {
        return false;
    }
    QSqlDatabase database = QSqlDatabase::database(kConnectionName);
    if (!ensureSchema(database))
    {
        closeConnection();
        return false;
    }
    m_CurrentFileName = fileName;
    return true;
}

void DatabaseManager::closeDatabase()
{
    closeConnection();
}

bool DatabaseManager::isOpen() const
{
    return !m_CurrentFileName.isEmpty() && QSqlDatabase::contains(kConnectionName);
}

QString DatabaseManager::currentDatabaseName() const
{
    return m_CurrentFileName;
}

QString DatabaseManager::getSetting(const QString& key, const QString& defaultValue) const
{
    if (!isOpen())
    {
        return defaultValue;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next())
    {
        return query.value(0).toString();
    }
    return defaultValue;
}

bool DatabaseManager::setSetting(const QString& key, const QString& value)
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

QList<DatabaseManager::UserInfo> DatabaseManager::queryUsers(const QString& keyword) const
{
    QList<UserInfo> users;
    if (!isOpen())
    {
        return users;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty())
    {
        query.prepare(QStringLiteral(
            "SELECT id, username, remark, is_enabled, created_at FROM users ORDER BY id"));
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT id, username, remark, is_enabled, created_at FROM users "
            "WHERE username LIKE ? OR remark LIKE ? ORDER BY id"));
        const QString pattern = QStringLiteral("%") + trimmedKeyword + QStringLiteral("%");
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    if (!query.exec())
    {
        return users;
    }
    while (query.next())
    {
        UserInfo info;
        info.id = query.value(0).toInt();
        info.username = query.value(1).toString();
        info.remark = query.value(2).toString();
        info.isEnabled = query.value(3).toInt() != 0;
        info.createdAt = query.value(4).toString();
        users.append(info);
    }
    return users;
}

bool DatabaseManager::addUser(const QString& username, const QString& password,
                              const QString& remark, bool isEnabled, QString* errorMessage)
{
    const QString name = username.trimmed();
    if (name.isEmpty())
    {
        setError(errorMessage, QStringLiteral("用户名不能为空"));
        return false;
    }
    if (password.isEmpty())
    {
        setError(errorMessage, QStringLiteral("密码不能为空"));
        return false;
    }
    if (userExists(name))
    {
        setError(errorMessage, QStringLiteral("用户名已存在"));
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO users (username, password_hash, remark, is_enabled, created_at) "
        "VALUES (?, ?, ?, ?, ?)"));
    query.addBindValue(name);
    query.addBindValue(hashPassword(password));
    query.addBindValue(remark.trimmed());
    query.addBindValue(isEnabled ? 1 : 0);
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::updateUser(int id, const QString& newUsername, const QString& newPassword,
                                 const QString& remark, bool isEnabled, QString* errorMessage)
{
    const QString name = newUsername.trimmed();
    if (name.isEmpty())
    {
        setError(errorMessage, QStringLiteral("用户名不能为空"));
        return false;
    }
    if (!isOpen())
    {
        setError(errorMessage, QStringLiteral("未打开数据库"));
        return false;
    }

    // 重名检查（排除自身）
    {
        QSqlQuery checkQuery(QSqlDatabase::database(kConnectionName));
        checkQuery.prepare(QStringLiteral("SELECT 1 FROM users WHERE username = ? AND id != ?"));
        checkQuery.addBindValue(name);
        checkQuery.addBindValue(id);
        if (checkQuery.exec() && checkQuery.next())
        {
            setError(errorMessage, QStringLiteral("用户名已存在"));
            return false;
        }
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (newPassword.isEmpty())
    {
        query.prepare(QStringLiteral(
            "UPDATE users SET username = ?, remark = ?, is_enabled = ? WHERE id = ?"));
        query.addBindValue(name);
        query.addBindValue(remark.trimmed());
        query.addBindValue(isEnabled ? 1 : 0);
        query.addBindValue(id);
    }
    else
    {
        query.prepare(QStringLiteral(
            "UPDATE users SET username = ?, password_hash = ?, remark = ?, is_enabled = ? WHERE id = ?"));
        query.addBindValue(name);
        query.addBindValue(hashPassword(newPassword));
        query.addBindValue(remark.trimmed());
        query.addBindValue(isEnabled ? 1 : 0);
        query.addBindValue(id);
    }
    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::deleteUser(int id)
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("DELETE FROM users WHERE id = ?"));
    query.addBindValue(id);
    return query.exec();
}

bool DatabaseManager::userExists(const QString& username) const
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("SELECT 1 FROM users WHERE username = ?"));
    query.addBindValue(username.trimmed());
    return query.exec() && query.next();
}

QString DatabaseManager::hashPassword(const QString& password)
{
    const QByteArray salt = randomBytes(16);
    const QByteArray digest = QCryptographicHash::hash(salt + password.toUtf8(), QCryptographicHash::Sha256).toHex();
    QByteArray result = salt.toHex();
    result += ':';
    result += digest;
    return QString::fromLatin1(result);
}

bool DatabaseManager::openConnection(const QString& fileName)
{
    closeConnection();
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    database.setDatabaseName(dataDirectory() + QLatin1Char('/') + fileName);
    if (!database.open())
    {
        QSqlDatabase::removeDatabase(kConnectionName);
        return false;
    }
    return true;
}

void DatabaseManager::closeConnection()
{
    m_CurrentFileName.clear();
    if (QSqlDatabase::contains(kConnectionName))
    {
        QSqlDatabase::database(kConnectionName).close();
        QSqlDatabase::removeDatabase(kConnectionName);
    }
}

bool DatabaseManager::ensureSchema(QSqlDatabase& database) const
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " username TEXT NOT NULL UNIQUE,"
            " password_hash TEXT NOT NULL,"
            " remark TEXT NOT NULL DEFAULT '',"
            " is_enabled INTEGER NOT NULL DEFAULT 1,"
            " created_at TEXT NOT NULL"
            ")")))
    {
        return false;
    }
    return query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS settings ("
        " key TEXT PRIMARY KEY,"
        " value TEXT NOT NULL"
        ")"));
}

void DatabaseManager::setError(QString* errorMessage, const QString& text)
{
    if (errorMessage)
    {
        *errorMessage = text;
    }
}
