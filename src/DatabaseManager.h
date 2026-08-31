#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QList>
#include <QObject>
#include <QString>

class QSqlDatabase;

// 账号数据库管理器：
// - 数据库文件存放在 <程序运行目录>/data 下，文件名是随机生成的 SHA256（64 位十六进制）
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
        bool isEnabled = true;
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
                 bool isEnabled, QString* errorMessage = nullptr);
    bool updateUser(int id, const QString& newUsername, const QString& newPassword,
                    const QString& remark, bool isEnabled, QString* errorMessage = nullptr);
    bool deleteUser(int id);
    bool userExists(const QString& username) const;

    // ---- 工具 ----
    static QString hashPassword(const QString& password); // 加盐 SHA-256，格式 "盐:摘要"

private:
    bool openConnection(const QString& fileName);
    void closeConnection();
    bool ensureSchema(QSqlDatabase& database) const;
    static void setError(QString* errorMessage, const QString& text);

    QString m_CurrentFileName;
};

#endif // DATABASEMANAGER_H
