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

#include "PortChecker.h"

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
    const QString basePath = dataDirectory() + QLatin1Char('/') + fileName;
    const bool ok = QFile::remove(basePath);
    // WAL 模式会产生 -wal/-shm 伴随文件，一并清理
    QFile::remove(basePath + QStringLiteral("-wal"));
    QFile::remove(basePath + QStringLiteral("-shm"));
    return ok;
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
            "SELECT id, username, remark, is_enabled, expire_at, created_at,"
            " remote_port_min, remote_port_max, local_port_min, local_port_max, max_port_count"
            " FROM users ORDER BY id"));
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT id, username, remark, is_enabled, expire_at, created_at,"
            " remote_port_min, remote_port_max, local_port_min, local_port_max, max_port_count"
            " FROM users WHERE username LIKE ? OR remark LIKE ? ORDER BY id"));
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
        info.remotePortMin = query.value(6).toInt();
        info.remotePortMax = query.value(7).toInt();
        info.localPortMin = query.value(8).toInt();
        info.localPortMax = query.value(9).toInt();
        info.maxPortCount = query.value(10).toInt();
        users.append(info);
    }
    return users;
}

bool DatabaseManager::setUserQuota(int userId, int remotePortMin, int remotePortMax,
                                   int localPortMin, int localPortMax, int maxPortCount)
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "UPDATE users SET remote_port_min = ?, remote_port_max = ?,"
        " local_port_min = ?, local_port_max = ?, max_port_count = ? WHERE id = ?"));
    query.addBindValue(remotePortMin);
    query.addBindValue(remotePortMax);
    query.addBindValue(localPortMin);
    query.addBindValue(localPortMax);
    query.addBindValue(maxPortCount);
    query.addBindValue(userId);
    return query.exec();
}

bool DatabaseManager::loadUserQuota(int userId, UserInfo* user) const
{
    if (!isOpen() || !user)
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "SELECT remote_port_min, remote_port_max, local_port_min, local_port_max, max_port_count"
        " FROM users WHERE id = ?"));
    query.addBindValue(userId);
    if (!query.exec() || !query.next())
    {
        return false;
    }
    user->remotePortMin = query.value(0).toInt();
    user->remotePortMax = query.value(1).toInt();
    user->localPortMin = query.value(2).toInt();
    user->localPortMax = query.value(3).toInt();
    user->maxPortCount = query.value(4).toInt();
    return true;
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

    // 端口配额校验：远端/本地端口必须在服务端界定的范围内，tcp/udp 数量不得超过上限
    UserInfo quotaUser;
    if (!loadUserQuota(userId, &quotaUser))
    {
        setError(errorMessage, QStringLiteral("用户不存在"));
        return false;
    }
    if (!isHttpLike
        && (remotePort < quotaUser.remotePortMin || remotePort > quotaUser.remotePortMax))
    {
        setError(errorMessage, QStringLiteral("远端端口 %1 超出该用户允许范围 %2-%3")
                                .arg(remotePort)
                                .arg(quotaUser.remotePortMin)
                                .arg(quotaUser.remotePortMax));
        return false;
    }
    if (localPort < quotaUser.localPortMin || localPort > quotaUser.localPortMax)
    {
        setError(errorMessage, QStringLiteral("本地端口 %1 超出该用户允许范围 %2-%3")
                                .arg(localPort)
                                .arg(quotaUser.localPortMin)
                                .arg(quotaUser.localPortMax));
        return false;
    }
    if (!isHttpLike)
    {
        QSqlQuery countQuery(QSqlDatabase::database(kConnectionName));
        countQuery.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM tunnels WHERE user_id = ? AND is_enabled = 1"
            " AND protocol IN ('tcp', 'udp')"));
        countQuery.addBindValue(userId);
        int usedCount = 0;
        if (countQuery.exec() && countQuery.next())
        {
            usedCount = countQuery.value(0).toInt();
        }
        if (usedCount + 1 > quotaUser.maxPortCount)
        {
            setError(errorMessage, QStringLiteral("该用户端口数量已达上限 %1（当前已用 %2）")
                                    .arg(quotaUser.maxPortCount)
                                    .arg(usedCount));
            return false;
        }

        // 端口占用检测：① 库内唯一（任何用户、含禁用中的隧道都不得复用同一远端端口）
        if (remotePortInUse(remotePort, -1))
        {
            setError(errorMessage, QStringLiteral("远端端口 %1 已被其他隧道占用，请更换端口")
                                    .arg(remotePort));
            return false;
        }
        // ② 本机监听检测：端口可能被其他程序占用
        if (!PortChecker::isPortFree(static_cast<quint16>(remotePort), tunnelProtocol))
        {
            setError(errorMessage, QStringLiteral("远端端口 %1 当前被本机其他程序占用，请更换端口")
                                    .arg(remotePort));
            return false;
        }
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

    // 同用户重名检查（排除自身）；同时取出隧道当前远端端口（用于占用检测）
    int tunnelUserId = -1;
    int currentRemotePort = 0;
    {
        QSqlQuery checkQuery(QSqlDatabase::database(kConnectionName));
        checkQuery.prepare(QStringLiteral(
            "SELECT user_id, remote_port FROM tunnels WHERE id = ?"));
        checkQuery.addBindValue(id);
        if (checkQuery.exec() && checkQuery.next())
        {
            tunnelUserId = checkQuery.value(0).toInt();
            currentRemotePort = checkQuery.value(1).toInt();
        }
    }
    {
        QSqlQuery checkQuery(QSqlDatabase::database(kConnectionName));
        checkQuery.prepare(QStringLiteral(
            "SELECT 1 FROM tunnels WHERE user_id = ? AND name = ? AND id != ?"));
        checkQuery.addBindValue(tunnelUserId);
        checkQuery.addBindValue(tunnelName);
        checkQuery.addBindValue(id);
        if (checkQuery.exec() && checkQuery.next())
        {
            setError(errorMessage, QStringLiteral("该用户下已存在同名隧道"));
            return false;
        }
    }

    // 端口配额校验：远端/本地端口必须在服务端界定的范围内，tcp/udp 数量不得超过上限
    UserInfo quotaUser;
    if (!loadUserQuota(tunnelUserId, &quotaUser))
    {
        setError(errorMessage, QStringLiteral("用户不存在"));
        return false;
    }
    if (!isHttpLike
        && (remotePort < quotaUser.remotePortMin || remotePort > quotaUser.remotePortMax))
    {
        setError(errorMessage, QStringLiteral("远端端口 %1 超出该用户允许范围 %2-%3")
                                .arg(remotePort)
                                .arg(quotaUser.remotePortMin)
                                .arg(quotaUser.remotePortMax));
        return false;
    }
    if (localPort < quotaUser.localPortMin || localPort > quotaUser.localPortMax)
    {
        setError(errorMessage, QStringLiteral("本地端口 %1 超出该用户允许范围 %2-%3")
                                .arg(localPort)
                                .arg(quotaUser.localPortMin)
                                .arg(quotaUser.localPortMax));
        return false;
    }
    if (!isHttpLike)
    {
        QSqlQuery countQuery(QSqlDatabase::database(kConnectionName));
        countQuery.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM tunnels WHERE user_id = ? AND is_enabled = 1"
            " AND protocol IN ('tcp', 'udp') AND id != ?"));
        countQuery.addBindValue(tunnelUserId);
        countQuery.addBindValue(id);
        int usedCount = 0;
        if (countQuery.exec() && countQuery.next())
        {
            usedCount = countQuery.value(0).toInt();
        }
        if (usedCount + 1 > quotaUser.maxPortCount)
        {
            setError(errorMessage, QStringLiteral("该用户端口数量已达上限 %1（当前已用 %2）")
                                    .arg(quotaUser.maxPortCount)
                                    .arg(usedCount));
            return false;
        }

        // 端口占用检测：① 库内唯一（排除自身；任何用户、含禁用中的隧道都不得复用）
        if (remotePortInUse(remotePort, id))
        {
            setError(errorMessage, QStringLiteral("远端端口 %1 已被其他隧道占用，请更换端口")
                                    .arg(remotePort));
            return false;
        }
        // ② 本机监听检测：与自身当前端口相同（隧道自身在 frps 上合法占用）时跳过
        if (remotePort != currentRemotePort
            && !PortChecker::isPortFree(static_cast<quint16>(remotePort), tunnelProtocol))
        {
            setError(errorMessage, QStringLiteral("远端端口 %1 当前被本机其他程序占用，请更换端口")
                                    .arg(remotePort));
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

bool DatabaseManager::remotePortInUse(int remotePort, int excludeTunnelId) const
{
    if (!isOpen())
    {
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (excludeTunnelId < 0)
    {
        query.prepare(QStringLiteral(
            "SELECT 1 FROM tunnels WHERE remote_port = ? AND protocol IN ('tcp', 'udp')"));
        query.addBindValue(remotePort);
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT 1 FROM tunnels WHERE remote_port = ? AND protocol IN ('tcp', 'udp') AND id != ?"));
        query.addBindValue(remotePort);
        query.addBindValue(excludeTunnelId);
    }
    return query.exec() && query.next();
}

bool DatabaseManager::addTraffic(int userId, const QString& userName, int tunnelId,
                                 const QString& tunnelName, const QString& recordDate,
                                 qint64 bytesIn, qint64 bytesOut)
{
    if (!isOpen())
    {
        return false;
    }
    if (bytesIn <= 0 && bytesOut <= 0)
    {
        return true; // 无增量，跳过
    }
    // 单条 UPSERT 原子累加：并发下不会丢累加值（避免先 UPDATE 后 INSERT 的两步法）
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO traffic_records"
        " (user_id, user_name, tunnel_id, tunnel_name, record_date, bytes_in, bytes_out)"
        " VALUES (?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(user_id, tunnel_id, record_date) DO UPDATE SET"
        " bytes_in = bytes_in + excluded.bytes_in,"
        " bytes_out = bytes_out + excluded.bytes_out"));
    query.addBindValue(userId);
    query.addBindValue(userName.trimmed().isEmpty() ? QStringLiteral("(已删除用户)") : userName.trimmed());
    query.addBindValue(tunnelId);
    query.addBindValue(tunnelName.trimmed().isEmpty() ? QStringLiteral("(已删除隧道)") : tunnelName.trimmed());
    query.addBindValue(recordDate);
    query.addBindValue(bytesIn);
    query.addBindValue(bytesOut);
    return query.exec();
}

QList<DatabaseManager::TrafficSummary> DatabaseManager::queryTunnelTraffic(int userId,
                                                                           const QString& dateFrom,
                                                                           const QString& dateTo) const
{
    QList<TrafficSummary> summaries;
    if (!isOpen())
    {
        return summaries;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (dateFrom.trimmed().isEmpty() && dateTo.trimmed().isEmpty())
    {
        // 按 tunnel_id 稳定排序：页面增量更新依赖行序稳定（避免流量变化导致行跳动）
        query.prepare(QStringLiteral(
            "SELECT tunnel_id, MAX(tunnel_name), SUM(bytes_in), SUM(bytes_out)"
            " FROM traffic_records WHERE user_id = ? GROUP BY tunnel_id ORDER BY tunnel_id"));
        query.addBindValue(userId);
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT tunnel_id, MAX(tunnel_name), SUM(bytes_in), SUM(bytes_out)"
            " FROM traffic_records WHERE user_id = ? AND record_date BETWEEN ? AND ?"
            " GROUP BY tunnel_id ORDER BY tunnel_id"));
        query.addBindValue(userId);
        query.addBindValue(dateFrom.trimmed());
        query.addBindValue(dateTo.trimmed());
    }
    if (!query.exec())
    {
        return summaries;
    }
    while (query.next())
    {
        TrafficSummary summary;
        summary.id = query.value(0).toInt();
        summary.name = query.value(1).toString();
        summary.bytesIn = query.value(2).toLongLong();
        summary.bytesOut = query.value(3).toLongLong();
        summaries.append(summary);
    }
    return summaries;
}

QList<DatabaseManager::TrafficSummary> DatabaseManager::queryUserTraffic(const QString& dateFrom,
                                                                         const QString& dateTo) const
{
    QList<TrafficSummary> summaries;
    if (!isOpen())
    {
        return summaries;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    if (dateFrom.trimmed().isEmpty() && dateTo.trimmed().isEmpty())
    {
        query.prepare(QStringLiteral(
            "SELECT user_id, MAX(user_name), SUM(bytes_in), SUM(bytes_out)"
            " FROM traffic_records GROUP BY user_id ORDER BY user_id"));
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT user_id, MAX(user_name), SUM(bytes_in), SUM(bytes_out)"
            " FROM traffic_records WHERE record_date BETWEEN ? AND ?"
            " GROUP BY user_id ORDER BY user_id"));
        query.addBindValue(dateFrom.trimmed());
        query.addBindValue(dateTo.trimmed());
    }
    if (!query.exec())
    {
        return summaries;
    }
    while (query.next())
    {
        TrafficSummary summary;
        summary.id = query.value(0).toInt();
        summary.name = query.value(1).toString();
        summary.bytesIn = query.value(2).toLongLong();
        summary.bytesOut = query.value(3).toLongLong();
        summaries.append(summary);
    }
    return summaries;
}

DatabaseManager::TrafficSummary DatabaseManager::queryUserTotalTraffic(int userId) const
{
    TrafficSummary summary;
    if (!isOpen())
    {
        return summary;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "SELECT MAX(user_name), SUM(bytes_in), SUM(bytes_out)"
        " FROM traffic_records WHERE user_id = ?"));
    query.addBindValue(userId);
    if (query.exec() && query.next())
    {
        summary.name = query.value(0).toString();
        summary.bytesIn = query.value(1).toLongLong();
        summary.bytesOut = query.value(2).toLongLong();
    }
    return summary;
}

QStringList DatabaseManager::queryTrafficTunnelNames(int userId) const
{
    QStringList names;
    if (!isOpen())
    {
        return names;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "SELECT DISTINCT tunnel_name FROM traffic_records WHERE user_id = ? ORDER BY tunnel_name"));
    query.addBindValue(userId);
    if (query.exec())
    {
        while (query.next())
        {
            names.append(query.value(0).toString());
        }
    }
    return names;
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

bool DatabaseManager::verifyPassword(const QString& password, const QString& storedHash)
{
    const QStringList parts = storedHash.split(QLatin1Char(':'));
    if (parts.size() != 2)
    {
        return false;
    }
    const QByteArray salt = QByteArray::fromHex(parts[0].toLatin1());
    const QByteArray expected = QByteArray::fromHex(parts[1].toLatin1());
    const QByteArray digest = QCryptographicHash::hash(salt + password.toUtf8(), QCryptographicHash::Sha256);
    return digest.compare(expected, Qt::CaseInsensitive) == 0;
}

DatabaseManager::LoginResult DatabaseManager::verifyUserLogin(const QString& username,
                                                              const QString& password,
                                                              UserInfo* user) const
{
    if (!isOpen())
    {
        return LoginResult::UserNotFound;
    }
    QSqlQuery query(QSqlDatabase::database(kConnectionName));
    query.prepare(QStringLiteral(
        "SELECT id, username, password_hash, is_enabled, expire_at, remark, created_at,"
        " remote_port_min, remote_port_max, local_port_min, local_port_max, max_port_count"
        " FROM users WHERE username = ?"));
    query.addBindValue(username.trimmed());
    if (!query.exec() || !query.next())
    {
        return LoginResult::UserNotFound;
    }
    const QString storedHash = query.value(2).toString();
    if (!verifyPassword(password, storedHash))
    {
        return LoginResult::WrongPassword;
    }
    if (query.value(3).toInt() == 0)
    {
        return LoginResult::Disabled;
    }
    const QString expireAt = query.value(4).toString();
    if (!expireAt.isEmpty())
    {
        const QDate expireDate = QDate::fromString(expireAt, QStringLiteral("yyyy-MM-dd"));
        if (expireDate.isValid() && expireDate < QDate::currentDate())
        {
            return LoginResult::Expired;
        }
    }

    if (user)
    {
        user->id = query.value(0).toInt();
        user->username = query.value(1).toString();
        user->isEnabled = true;
        user->expireAt = expireAt;
        user->remark = query.value(5).toString();
        user->createdAt = query.value(6).toString();
        user->remotePortMin = query.value(7).toInt();
        user->remotePortMax = query.value(8).toInt();
        user->localPortMin = query.value(9).toInt();
        user->localPortMax = query.value(10).toInt();
        user->maxPortCount = query.value(11).toInt();
    }
    return LoginResult::Ok;
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
    // 多连接并发（面板各页面/流量监控同时读写同一文件）：
    // WAL 允许读写并行，busy_timeout 避免瞬时锁冲突导致查询失败
    database.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    database.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
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
            " created_at TEXT NOT NULL,"
            " remote_port_min INTEGER NOT NULL DEFAULT 10000,"
            " remote_port_max INTEGER NOT NULL DEFAULT 60000,"
            " local_port_min INTEGER NOT NULL DEFAULT 1024,"
            " local_port_max INTEGER NOT NULL DEFAULT 65535,"
            " max_port_count INTEGER NOT NULL DEFAULT 10"
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

    // 旧库迁移：早期版本 users 表缺少列时自动补充
    struct ColumnSpec
    {
        const char* name;
        const char* ddl;
    };
    static const ColumnSpec kMissingColumns[] = {
        {"expire_at", "expire_at TEXT NOT NULL DEFAULT ''"},
        {"remote_port_min", "remote_port_min INTEGER NOT NULL DEFAULT 10000"},
        {"remote_port_max", "remote_port_max INTEGER NOT NULL DEFAULT 60000"},
        {"local_port_min", "local_port_min INTEGER NOT NULL DEFAULT 1024"},
        {"local_port_max", "local_port_max INTEGER NOT NULL DEFAULT 65535"},
        {"max_port_count", "max_port_count INTEGER NOT NULL DEFAULT 10"},
    };

    QStringList existingColumns;
    QSqlQuery pragmaQuery(database);
    if (pragmaQuery.exec(QStringLiteral("PRAGMA table_info(users)")))
    {
        while (pragmaQuery.next())
        {
            existingColumns.append(pragmaQuery.value(1).toString());
        }
    }
    for (const ColumnSpec& column : kMissingColumns)
    {
        if (!existingColumns.contains(QLatin1String(column.name)))
        {
            QSqlQuery alterQuery(database);
            if (!alterQuery.exec(QStringLiteral("ALTER TABLE users ADD COLUMN ") + QLatin1String(column.ddl)))
            {
                return false;
            }
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
    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_tunnels_user ON tunnels(user_id)")))
    {
        return false;
    }

    // 流量记录表：用户/隧道删除后记录仍保留（名称快照），无外键
    return query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS traffic_records ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " user_name TEXT NOT NULL,"
        " tunnel_id INTEGER NOT NULL,"
        " tunnel_name TEXT NOT NULL,"
        " record_date TEXT NOT NULL,"
        " bytes_in INTEGER NOT NULL DEFAULT 0,"
        " bytes_out INTEGER NOT NULL DEFAULT 0,"
        " UNIQUE(user_id, tunnel_id, record_date)"
        ")"));
}

void DatabaseManager::setError(QString* errorMessage, const QString& text)
{
    if (errorMessage)
    {
        *errorMessage = text;
    }
}
