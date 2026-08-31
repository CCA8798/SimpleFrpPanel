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

// Qt SQLite 驱动会把"空 QString"绑定为 SQL NULL，而 remark/expire_at 列是 NOT NULL，
// 因此空值必须显式转换为"非空空字符串"再绑定
QVariant bindText(const QString& text)
{
    const QString trimmed = text.trimmed();
    return QVariant(trimmed.isEmpty() ? QStringLiteral("") : trimmed);
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

    // 随机生成 SHA256 文件名（截取前 10 位十六进制），冲突则重新生成
    QString fileName;
    while (true)
    {
        const QString candidate = QString::fromLatin1(
                                      QCryptographicHash::hash(randomBytes(32), QCryptographicHash::Sha256).toHex().left(10))
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
            "SELECT id, username, remark, is_enabled, expire_at, created_at FROM users ORDER BY id"));
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT id, username, remark, is_enabled, expire_at, created_at FROM users "
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
        info.expireAt = query.value(4).toString();
        info.createdAt = query.value(5).toString();
        users.append(info);
    }
    return users;
}

bool DatabaseManager::addUser(const QString& username, const QString& password,
                              const QString& remark, bool isEnabled, const QString& expireAt,
                              QString* errorMessage)
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
        "INSERT INTO users (username, password_hash, remark, is_enabled, expire_at, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(name);
    query.addBindValue(hashPassword(password));
    query.addBindValue(bindText(remark));
    query.addBindValue(isEnabled ? 1 : 0);
    query.addBindValue(bindText(expireAt));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::updateUser(int id, const QString& newUsername, const QString& newPassword,
                                 const QString& remark, bool isEnabled, const QString& expireAt,
                                 QString* errorMessage)
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
            "UPDATE users SET username = ?, remark = ?, is_enabled = ?, expire_at = ? WHERE id = ?"));
        query.addBindValue(name);
        query.addBindValue(bindText(remark));
        query.addBindValue(isEnabled ? 1 : 0);
        query.addBindValue(bindText(expireAt));
        query.addBindValue(id);
    }
    else
    {
        query.prepare(QStringLiteral(
            "UPDATE users SET username = ?, password_hash = ?, remark = ?, is_enabled = ?, expire_at = ? WHERE id = ?"));
        query.addBindValue(name);
        query.addBindValue(hashPassword(newPassword));
        query.addBindValue(bindText(remark));
        query.addBindValue(isEnabled ? 1 : 0);
        query.addBindValue(bindText(expireAt));
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
            " expire_at TEXT NOT NULL DEFAULT '',"
            " created_at TEXT NOT NULL"
            ")")))
    {
        return false;
    }
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings ("
            " key TEXT PRIMARY KEY,"
            " value TEXT NOT NULL"
            ")")))
    {
        return false;
    }

    // 旧库迁移：早期版本 users 表没有 expire_at 列，自动补充
    QSqlQuery pragmaQuery(database);
    bool hasExpireAt = false;
    if (pragmaQuery.exec(QStringLiteral("PRAGMA table_info(users)")))
    {
        while (pragmaQuery.next())
        {
            if (pragmaQuery.value(1).toString() == QStringLiteral("expire_at"))
            {
                hasExpireAt = true;
                break;
            }
        }
    }
    if (!hasExpireAt)
    {
        QSqlQuery alterQuery(database);
        if (!alterQuery.exec(QStringLiteral(
                "ALTER TABLE users ADD COLUMN expire_at TEXT NOT NULL DEFAULT ''")))
        {
            return false;
        }
    }
    return true;
}

void DatabaseManager::setError(QString* errorMessage, const QString& text)
{
    if (errorMessage)
    {
        *errorMessage = text;
    }
}
