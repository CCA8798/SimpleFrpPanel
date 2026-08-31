#ifndef PANELAPISERVER_H
#define PANELAPISERVER_H

#include <QHash>
#include <QObject>
#include <QString>

class DatabaseManager;
class FrpsManager;
class QTcpServer;
class QTcpSocket;
class QJsonObject;

// 面板 API 服务端：为客户端隧道管理提供 TCP + JSON 行协议
//   login / logout / list / add_tunnel / update_tunnel / delete_tunnel / set_tunnel_enabled
// 认证：用户名+密码（数据库校验），登录成功签发随机 token，后续命令携带 token
// 强制逻辑：配额（端口范围/数量上限）由 DatabaseManager 统一校验
class PanelApiServer : public QObject
{
    Q_OBJECT

public:
    explicit PanelApiServer(DatabaseManager* databaseManager, FrpsManager* frpsManager,
                            QObject* parent = nullptr);

    bool start(quint16 port, QString* errorMessage = nullptr);
    void stop();
    bool isRunning() const;
    quint16 port() const;
    void disconnectAllClients(); // 数据库切换时调用，踢掉所有在线客户端

Q_SIGNALS:
    void runningChanged(bool running);
    void logMessage(const QString& text);

private:
    void onNewConnection();
    void onClientDisconnected();
    void handleRequest(QTcpSocket* socket, const QJsonObject& request);
    void sendResponse(QTcpSocket* socket, const QJsonObject& response);
    bool tunnelBelongsToUser(int tunnelId, int userId) const;
    QString issueToken(int userId);
    int userIdByToken(const QString& token) const;

    QTcpServer* m_Server = nullptr;
    DatabaseManager* m_DatabaseManager = nullptr;
    FrpsManager* m_FrpsManager = nullptr;
    QHash<QString, int> m_TokenToUserId;
};

#endif // PANELAPISERVER_H
