#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QList>
#include <QObject>
#include <QString>

class QSqlDatabase;

// 账号数据库管理器：
// - 数据库文件存放在 <程序运行目录>/data 下，文件名是随机 SHA256 截取前 10 位（十六进制）
// - 每个 .db 包含 users 表（账号）与 settings 表（键值设置，如绑定的公网 IP/端口）
// - 同一时刻只打开一个数据库，即"当前数据库"
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    struct UserInfo
    {
        int id = 0;
        QString username;
        QString remark;
        QString createdAt;
        QString expireAt; // "yyyy-MM-dd"，空串表示永不过期
        bool isEnabled = true;
        // 端口配额（服务端界定范围与上限，具体端口由客户端在范围内自选）
        int remotePortMin = 10000;
        int remotePortMax = 60000;
        int localPortMin = 1024;
        int localPortMax = 65535;
        int maxPortCount = 10; // 最多可同时使用的端口数（仅 tcp/udp 计入）
    };

    struct TunnelInfo
    {
        int id = 0;
        int userId = 0;
        QString name;
        QString protocol; // tcp / udp / http / https
        int remotePort = 0;
        QString localIp;
        int localPort = 0;
        QString customDomain; // http/https 使用
        bool isEnabled = true;
        QString remark;
        QString createdAt;
    };

    struct TrafficSummary
    {
        int id = 0; // 用户 id 或隧道 id
        QString name; // 快照名（删除后仍可显示）
        qint64 bytesIn = 0;
        qint64 bytesOut = 0;
    };

    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager() override;

    // ---- 数据库文件管理 ----
    static QString dataDirectory();             // <程序运行目录>/data
    QStringList databaseFileNames() const;      // 所有 .db 文件名（按名称排序）
    QString createDatabase();                   // 新建并初始化，返回文件名；失败返回空串
    bool deleteDatabase(const QString& fileName);
    bool openDatabase(const QString& fileName); // 打开为当前数据库
    void closeDatabase();
    bool isOpen() const;
    QString currentDatabaseName() const;

    // ---- 设置（当前数据库，键值对）----
    QString getSetting(const QString& key, const QString& defaultValue = QString()) const;
    bool setSetting(const QString& key, const QString& value);

    // ---- 用户 CRUD（当前数据库）----
    QList<UserInfo> queryUsers(const QString& keyword = QString()) const;
    bool addUser(const QString& username, const QString& password, const QString& remark,
                 bool isEnabled, const QString& expireAt, QString* errorMessage = nullptr);
    bool updateUser(int id, const QString& newUsername, const QString& newPassword,
                    const QString& remark, bool isEnabled, const QString& expireAt,
                    QString* errorMessage = nullptr);
    bool deleteUser(int id);
    bool userExists(const QString& username) const;

    // ---- 端口配额（当前数据库）----
    // 服务端为用户界定：远端端口范围 / 本地端口范围 / 最大可用端口数；
    // 隧道 CRUD 时自动校验（端口必须在范围内、tcp/udp 数量不得超过上限）
    bool setUserQuota(int userId, int remotePortMin, int remotePortMax,
                      int localPortMin, int localPortMax, int maxPortCount);

    // ---- 隧道 CRUD（当前数据库）----
    QList<TunnelInfo> queryTunnels(int userId, const QString& keyword = QString()) const;
    bool addTunnel(int userId, const QString& name, const QString& protocol, int remotePort,
                   const QString& localIp, int localPort, const QString& customDomain,
                   bool isEnabled, const QString& remark, QString* errorMessage = nullptr);
    bool updateTunnel(int id, const QString& name, const QString& protocol, int remotePort,
                      const QString& localIp, int localPort, const QString& customDomain,
                      bool isEnabled, const QString& remark, QString* errorMessage = nullptr);
    bool deleteTunnel(int id);
    bool setTunnelEnabled(int id, bool isEnabled);
    bool tunnelNameExists(int userId, const QString& name, int excludeId = -1) const;

    // ---- 流量记录（当前数据库）----
    // 按（用户, 隧道, 日期）累加；用户/隧道删除后记录保留（名称快照）
    bool addTraffic(int userId, const QString& userName, int tunnelId, const QString& tunnelName,
                    const QString& recordDate, qint64 bytesIn, qint64 bytesOut);
    // 指定日期区间（空串 = 不限）内，按隧道聚合某用户的流量（含已删除隧道）
    QList<TrafficSummary> queryTunnelTraffic(int userId, const QString& dateFrom,
                                             const QString& dateTo) const;
    // 指定日期区间内，按用户聚合所有用户的流量（含已删除用户）
    QList<TrafficSummary> queryUserTraffic(const QString& dateFrom, const QString& dateTo) const;
    // 某用户全部历史流量的汇总（历史总流量）
    TrafficSummary queryUserTotalTraffic(int userId) const;
    // 某用户历史上出现过的隧道名（含已删除，用于查询下拉框）
    QStringList queryTrafficTunnelNames(int userId) const;

    // ---- 工具 ----
    static QString hashPassword(const QString& password); // 加盐 SHA-256，格式 "盐:摘要"
    static bool verifyPassword(const QString& password, const QString& storedHash);

    // 登录校验结果
    enum class LoginResult
    {
        Ok,
        UserNotFound,
        WrongPassword,
        Disabled,
        Expired,
    };
    // 校验用户名/密码（含启用状态与到期时间），成功时填充用户信息（含配额）
    LoginResult verifyUserLogin(const QString& username, const QString& password,
                                UserInfo* user) const;

private:
    bool openConnection(const QString& fileName);
    void closeConnection();
    bool ensureSchema(QSqlDatabase& database) const;
    bool loadUserQuota(int userId, UserInfo* user) const;
    // 远端端口是否已被库内其他隧道占用（任意用户，含禁用隧道）
    bool remotePortInUse(int remotePort, int excludeTunnelId) const;
    static void setError(QString* errorMessage, const QString& text);

    QString m_CurrentFileName;
};

#endif // DATABASEMANAGER_H
