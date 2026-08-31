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

QList<DatabaseManager::TunnelInfo> DatabaseManager::queryTunnels(int userId, const QString& keyword) const
{
    QList<TunnelInfo> tunnels;
    if (!isOpen())
    {
        return tunnels;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty())
    {
        query.prepare(QStringLiteral(
            "SELECT id, user_id, name, protocol, remote_port, local_ip, local_port,"
            " custom_domain, is_enabled, remark, created_at FROM tunnels WHERE user_id = ? ORDER BY id"));
        query.addBindValue(userId);
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT id, user_id, name, protocol, remote_port, local_ip, local_port,"
            " custom_domain, is_enabled, remark, created_at FROM tunnels"
            " WHERE user_id = ? AND (name LIKE ? OR remark LIKE ? OR local_ip LIKE ?"
            " OR custom_domain LIKE ?) ORDER BY id"));
        const QString pattern = QStringLiteral("%") + trimmedKeyword + QStringLiteral("%");
        query.addBindValue(userId);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
        query.addBindValue(pattern);
    }
    if (!query.exec())
    {
        return tunnels;
    }
    while (query.next())
    {
        TunnelInfo info;
        info.id = query.value(0).toInt();
        info.userId = query.value(1).toInt();
        info.name = query.value(2).toString();
        info.protocol = query.value(3).toString();
        info.remotePort = query.value(4).toInt();
        info.localIp = query.value(5).toString();
        info.localPort = query.value(6).toInt();
        info.customDomain = query.value(7).toString();
        info.isEnabled = query.value(8).toInt() != 0;
        info.remark = query.value(9).toString();
        info.createdAt = query.value(10).toString();
        tunnels.append(info);
    }
    return tunnels;
}

bool DatabaseManager::addTunnel(int userId, const QString& name, const QString& protocol,
                                int remotePort, const QString& localIp, int localPort,
                                const QString& customDomain, bool isEnabled,
                                const QString& remark, QString* errorMessage)
{
    if (!isOpen())
    {
        setError(errorMessage, QStringLiteral("未打开数据库"));
        return false;
    }
    const QString tunnelName = name.trimmed();
    if (tunnelName.isEmpty())
    {
        setError(errorMessage, QStringLiteral("隧道名称不能为空"));
        return false;
    }
    if (tunnelNameExists(userId, tunnelName))
    {
        setError(errorMessage, QStringLiteral("该用户下已存在同名隧道"));
        return false;
    }
    const QString tunnelProtocol = protocol.trimmed().isEmpty() ? QStringLiteral("tcp") : protocol.trimmed();
    const bool isHttpLike = (tunnelProtocol == QStringLiteral("http")
                             || tunnelProtocol == QStringLiteral("https"));
    if (!isHttpLike && (remotePort < 1 || remotePort > 65535))
    {
        setError(errorMessage, QStringLiteral("远端端口必须是 1-65535 的整数"));
        return false;
    }
    if (localPort < 1 || localPort > 65535)
    {
        setError(errorMessage, QStringLiteral("目标端口必须是 1-65535 的整数"));
        return false;
    }
    if (localIp.trimmed().isEmpty())
    {
        setError(errorMessage, QStringLiteral("目标内网 IP 不能为空"));
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO tunnels (user_id, name, protocol, remote_port, local_ip, local_port,"
        " custom_domain, is_enabled, remark, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(userId);
    query.addBindValue(tunnelName);
    query.addBindValue(tunnelProtocol);
    query.addBindValue(remotePort);
    query.addBindValue(bindText(localIp));
    query.addBindValue(localPort);
    query.addBindValue(bindText(customDomain));
    query.addBindValue(isEnabled ? 1 : 0);
    query.addBindValue(bindText(remark));
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!query.exec())
    {
        // 外键约束失败 = 用户不存在或已被删除，给出明确提示
        const QString errorText = query.lastError().text();
        if (errorText.contains(QStringLiteral("FOREIGN KEY")))
        {
            setError(errorMessage, QStringLiteral("用户不存在或已被删除，请刷新后重试"));
        }
        else
        {
            setError(errorMessage, errorText);
        }
        return false;
    }
    return true;
}

bool DatabaseManager::updateTunnel(int id, const QString& name, const QString& protocol,
                                   int remotePort, const QString& localIp, int localPort,
                                   const QString& customDomain, bool isEnabled,
                                   const QString& remark, QString* errorMessage)
{
    if (!isOpen())
    {
        setError(errorMessage, QStringLiteral("未打开数据库"));
        return false;
    }
    const QString tunnelName = name.trimmed();
    if (tunnelName.isEmpty())
    {
        setError(errorMessage, QStringLiteral("隧道名称不能为空"));
        return false;
    }
    const QString tunnelProtocol = protocol.trimmed().isEmpty() ? QStringLiteral("tcp") : protocol.trimmed();
    const bool isHttpLike = (tunnelProtocol == QStringLiteral("http")
                             || tunnelProtocol == QStringLiteral("https"));
    if (!isHttpLike && (remotePort < 1 || remotePort > 65535))
    {
        setError(errorMessage, QStringLiteral("远端端口必须是 1-65535 的整数"));
        return false;
    }
    if (localPort < 1 || localPort > 65535)
    {
        setError(errorMessage, QStringLiteral("目标端口必须是 1-65535 的整数"));
        return false;
    }
    if (localIp.trimmed().isEmpty())
    {
        setError(errorMessage, QStringLiteral("目标内网 IP 不能为空"));
        return false;
    }

    // 同用户重名检查（排除自身）
    {
        QSqlQuery checkQuery(QSqlDatabase::database(kConnectionName));
        checkQuery.prepare(QStringLiteral(
            "SELECT 1 FROM tunnels WHERE user_id = (SELECT user_id FROM tunnels WHERE id = ?) AND name = ? AND id != ?"));
        checkQuery.addBindValue(id);
        checkQuery.addBindValue(tunnelName);
        checkQuery.addBindValue(id);
        if (checkQuery.exec() && checkQuery.next())
        {
            setError(errorMessage, QStringLiteral("该用户下已存在同名隧道"));
            return false;
        }
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "UPDATE tunnels SET name = ?, protocol = ?, remote_port = ?, local_ip = ?,"
        " local_port = ?, custom_domain = ?, is_enabled = ?, remark = ? WHERE id = ?"));
    query.addBindValue(tunnelName);
    query.addBindValue(tunnelProtocol);
    query.addBindValue(remotePort);
    query.addBindValue(bindText(localIp));
    query.addBindValue(localPort);
    query.addBindValue(bindText(customDomain));
    query.addBindValue(isEnabled ? 1 : 0);
    query.addBindValue(bindText(remark));
    query.addBindValue(id);
    if (!query.exec())
    {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::deleteTunnel(int id)
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("DELETE FROM tunnels WHERE id = ?"));
    query.addBindValue(id);
    return query.exec();
}

bool DatabaseManager::setTunnelEnabled(int id, bool isEnabled)
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral("UPDATE tunnels SET is_enabled = ? WHERE id = ?"));
    query.addBindValue(isEnabled ? 1 : 0);
    query.addBindValue(id);
    return query.exec();
}

bool DatabaseManager::tunnelNameExists(int userId, const QString& name, int excludeId) const
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (excludeId < 0)
    {
        query.prepare(QStringLiteral("SELECT 1 FROM tunnels WHERE user_id = ? AND name = ?"));
        query.addBindValue(userId);
        query.addBindValue(name.trimmed());
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT 1 FROM tunnels WHERE user_id = ? AND name = ? AND id != ?"));
        query.addBindValue(userId);
        query.addBindValue(name.trimmed());
        query.addBindValue(excludeId);
    }
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
    // 启用外键约束：删除用户时级联删除其隧道
    database.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
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

    // 隧道表（服务端隧道管理）
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tunnels ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " user_id INTEGER NOT NULL,"
            " name TEXT NOT NULL,"
            " protocol TEXT NOT NULL DEFAULT 'tcp',"
            " remote_port INTEGER NOT NULL DEFAULT 0,"
            " local_ip TEXT NOT NULL DEFAULT '',"
            " local_port INTEGER NOT NULL DEFAULT 0,"
            " custom_domain TEXT NOT NULL DEFAULT '',"
            " is_enabled INTEGER NOT NULL DEFAULT 1,"
            " remark TEXT NOT NULL DEFAULT '',"
            " created_at TEXT NOT NULL,"
            " FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
            ")")))
    {
        return false;
    }
    return query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_tunnels_user ON tunnels(user_id)"));
}

void DatabaseManager::setError(QString* errorMessage, const QString& text)
{
    if (errorMessage)
    {
        *errorMessage = text;
    }
}
