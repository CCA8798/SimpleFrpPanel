#include "PanelApiServer.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>

#include "DatabaseManager.h"
#include "FrpsManager.h"

namespace {
const QString kSettingPublicIp = QStringLiteral("public_ip");
const QString kSettingPublicPort = QStringLiteral("public_port");

QString randomToken()
{
    QByteArray bytes;
    bytes.reserve(16);
    while (bytes.size() < 16)
    {
        const quint32 value = QRandomGenerator::system()->generate();
        bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    return QString::fromLatin1(bytes.left(16).toHex());
}

QJsonObject tunnelToJson(const DatabaseManager::TunnelInfo& tunnel, bool frpsRunning)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), tunnel.id);
    object.insert(QStringLiteral("name"), tunnel.name);
    object.insert(QStringLiteral("protocol"), tunnel.protocol);
    object.insert(QStringLiteral("remotePort"), tunnel.remotePort);
    object.insert(QStringLiteral("localIp"), tunnel.localIp);
    object.insert(QStringLiteral("localPort"), tunnel.localPort);
    object.insert(QStringLiteral("customDomain"), tunnel.customDomain);
    object.insert(QStringLiteral("enabled"), tunnel.isEnabled);
    object.insert(QStringLiteral("remark"), tunnel.remark);
    QString status;
    if (!tunnel.isEnabled)
    {
        status = QStringLiteral("已禁用");
    }
    else if (frpsRunning)
    {
        status = QStringLiteral("运行中");
    }
    else
    {
        status = QStringLiteral("未运行");
    }
    object.insert(QStringLiteral("status"), status);
    return object;
}

QJsonObject quotaToJson(const DatabaseManager::UserInfo& user, int usedCount)
{
    QJsonObject object;
    object.insert(QStringLiteral("remoteMin"), user.remotePortMin);
    object.insert(QStringLiteral("remoteMax"), user.remotePortMax);
    object.insert(QStringLiteral("localMin"), user.localPortMin);
    object.insert(QStringLiteral("localMax"), user.localPortMax);
    object.insert(QStringLiteral("maxCount"), user.maxPortCount);
    object.insert(QStringLiteral("usedCount"), usedCount);
    return object;
}

int countUsedPorts(DatabaseManager* manager, int userId)
{
    int count = 0;
    const QList<DatabaseManager::TunnelInfo> tunnels = manager->queryTunnels(userId);
    for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
    {
        if (tunnel.isEnabled
            && (tunnel.protocol == QStringLiteral("tcp") || tunnel.protocol == QStringLiteral("udp")))
        {
            ++count;
        }
    }
    return count;
}
} // namespace

PanelApiServer::PanelApiServer(DatabaseManager* databaseManager, FrpsManager* frpsManager,
                               QObject* parent)
    : QObject(parent)
    , m_DatabaseManager(databaseManager)
    , m_FrpsManager(frpsManager)
{
    m_Server = new QTcpServer(this);
    connect(m_Server, &QTcpServer::newConnection, this, &PanelApiServer::onNewConnection);
}

bool PanelApiServer::start(quint16 port, QString* errorMessage)
{
    if (isRunning())
    {
        return true;
    }
    if (!m_Server->listen(QHostAddress::Any, port))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("面板服务监听失败: %1").arg(m_Server->errorString());
        }
        return false;
    }
    emit runningChanged(true);
    return true;
}

void PanelApiServer::stop()
{
    if (!isRunning())
    {
        return;
    }
    disconnectAllClients();
    m_Server->close();
    emit runningChanged(false);
}

bool PanelApiServer::isRunning() const
{
    return m_Server->isListening();
}

quint16 PanelApiServer::port() const
{
    return m_Server->serverPort();
}

void PanelApiServer::disconnectAllClients()
{
    const QList<QTcpSocket*> clients = findChildren<QTcpSocket*>();
    for (QTcpSocket* socket : clients)
    {
        socket->disconnectFromHost();
    }
    m_TokenToUserId.clear();
}

void PanelApiServer::onNewConnection()
{
    while (m_Server->hasPendingConnections())
    {
        QTcpSocket* socket = m_Server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            while (socket->canReadLine())
            {
                const QByteArray line = socket->readLine().trimmed();
                if (line.isEmpty())
                {
                    continue;
                }
                const QJsonDocument document = QJsonDocument::fromJson(line);
                if (document.isObject())
                {
                    handleRequest(socket, document.object());
                }
                else
                {
                    sendResponse(socket, QJsonObject{
                                                {QStringLiteral("cmd"), QStringLiteral("error")},
                                                {QStringLiteral("message"), QStringLiteral("无效的请求格式")},
                                            });
                }
            }
        });
        connect(socket, &QTcpSocket::disconnected, this, &PanelApiServer::onClientDisconnected);
        emit logMessage(QStringLiteral("[%1] 客户端已连接: %2")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                 socket->peerAddress().toString()));
    }
}

void PanelApiServer::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
    {
        return;
    }
    // token 不绑定连接，断开时保留 token（过期由客户端管理）；这里仅记录日志
    emit logMessage(QStringLiteral("[%1] 客户端已断开: %2")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             socket->peerAddress().toString()));
    socket->deleteLater();
}

bool PanelApiServer::tunnelBelongsToUser(int tunnelId, int userId) const
{
    const QList<DatabaseManager::TunnelInfo> tunnels = m_DatabaseManager->queryTunnels(userId);
    for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
    {
        if (tunnel.id == tunnelId)
        {
            return true;
        }
    }
    return false;
}

void PanelApiServer::handleRequest(QTcpSocket* socket, const QJsonObject& request)
{
    const QString cmd = request.value(QStringLiteral("cmd")).toString();

    if (cmd == QStringLiteral("login"))
    {
        const QString username = request.value(QStringLiteral("username")).toString();
        const QString password = request.value(QStringLiteral("password")).toString();
        DatabaseManager::UserInfo user;
        const DatabaseManager::LoginResult result =
            m_DatabaseManager->verifyUserLogin(username, password, &user);
        if (result != DatabaseManager::LoginResult::Ok)
        {
            QString message = QStringLiteral("用户名或密码错误");
            if (result == DatabaseManager::LoginResult::Disabled)
            {
                message = QStringLiteral("该账号已被禁用");
            }
            else if (result == DatabaseManager::LoginResult::Expired)
            {
                message = QStringLiteral("该账号已过期");
            }
            sendResponse(socket, QJsonObject{
                                    {QStringLiteral("cmd"), QStringLiteral("login")},
                                    {QStringLiteral("ok"), false},
                                    {QStringLiteral("message"), message},
                                });
            return;
        }
        const QString token = issueToken(user.id);
        const QJsonObject serverInfo{
            {QStringLiteral("publicIp"), m_DatabaseManager->getSetting(kSettingPublicIp)},
            {QStringLiteral("publicPort"), m_DatabaseManager->getSetting(kSettingPublicPort)},
            // frpc 连接参数：frps 绑定端口与认证 token（仅登录成功后下发）
            {QStringLiteral("frpsBindPort"), m_DatabaseManager->getSetting(QStringLiteral("frps_bind_port"), QStringLiteral("7000"))},
            {QStringLiteral("frpsToken"), m_DatabaseManager->getSetting(QStringLiteral("frps_token"))},
        };
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), QStringLiteral("login")},
                                {QStringLiteral("ok"), true},
                                {QStringLiteral("token"), token},
                                {QStringLiteral("username"), user.username},
                                {QStringLiteral("quota"), quotaToJson(user, countUsedPorts(m_DatabaseManager, user.id))},
                                {QStringLiteral("serverInfo"), serverInfo},
                            });
        emit logMessage(QStringLiteral("[%1] 用户 %2 登录成功")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                 user.username));
        return;
    }

    const QString token = request.value(QStringLiteral("token")).toString();
    const int userId = userIdByToken(token);
    if (userId < 0)
    {
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), cmd},
                                {QStringLiteral("ok"), false},
                                {QStringLiteral("message"), QStringLiteral("未登录或登录已过期")},
                            });
        return;
    }

    if (cmd == QStringLiteral("logout"))
    {
        m_TokenToUserId.remove(token);
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), QStringLiteral("logout")},
                                {QStringLiteral("ok"), true},
                            });
        return;
    }

    if (cmd == QStringLiteral("list"))
    {
        const DatabaseManager::UserInfo* userPtr = nullptr;
        QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
        DatabaseManager::UserInfo currentUser;
        for (const DatabaseManager::UserInfo& user : users)
        {
            if (user.id == userId)
            {
                currentUser = user;
                userPtr = &currentUser;
                break;
            }
        }
        if (!userPtr)
        {
            sendResponse(socket, QJsonObject{
                                    {QStringLiteral("cmd"), cmd},
                                    {QStringLiteral("ok"), false},
                                    {QStringLiteral("message"), QStringLiteral("用户不存在")},
                                });
            return;
        }
        QJsonArray tunnels;
        const QList<DatabaseManager::TunnelInfo> tunnelList = m_DatabaseManager->queryTunnels(userId);
        const bool frpsRunning = m_FrpsManager->isRunning();
        for (const DatabaseManager::TunnelInfo& tunnel : tunnelList)
        {
            tunnels.append(tunnelToJson(tunnel, frpsRunning));
        }
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), QStringLiteral("list")},
                                {QStringLiteral("ok"), true},
                                {QStringLiteral("tunnels"), tunnels},
                                {QStringLiteral("quota"), quotaToJson(currentUser, countUsedPorts(m_DatabaseManager, userId))},
                                {QStringLiteral("frpsRunning"), frpsRunning},
                            });
        return;
    }

    if (cmd == QStringLiteral("add_tunnel"))
    {
        QString errorMessage;
        const bool ok = m_DatabaseManager->addTunnel(
            userId,
            request.value(QStringLiteral("name")).toString(),
            request.value(QStringLiteral("protocol")).toString(),
            request.value(QStringLiteral("remotePort")).toInt(),
            request.value(QStringLiteral("localIp")).toString(),
            request.value(QStringLiteral("localPort")).toInt(),
            request.value(QStringLiteral("customDomain")).toString(),
            request.value(QStringLiteral("enabled")).toBool(true),
            request.value(QStringLiteral("remark")).toString(),
            &errorMessage);
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), cmd},
                                {QStringLiteral("ok"), ok},
                                {QStringLiteral("message"), errorMessage},
                            });
        return;
    }

    if (cmd == QStringLiteral("update_tunnel"))
    {
        const int tunnelId = request.value(QStringLiteral("id")).toInt();
        if (!tunnelBelongsToUser(tunnelId, userId))
        {
            sendResponse(socket, QJsonObject{
                                    {QStringLiteral("cmd"), cmd},
                                    {QStringLiteral("ok"), false},
                                    {QStringLiteral("message"), QStringLiteral("隧道不存在或不属于当前用户")},
                                });
            return;
        }
        QString errorMessage;
        const bool ok = m_DatabaseManager->updateTunnel(
            tunnelId,
            request.value(QStringLiteral("name")).toString(),
            request.value(QStringLiteral("protocol")).toString(),
            request.value(QStringLiteral("remotePort")).toInt(),
            request.value(QStringLiteral("localIp")).toString(),
            request.value(QStringLiteral("localPort")).toInt(),
            request.value(QStringLiteral("customDomain")).toString(),
            request.value(QStringLiteral("enabled")).toBool(true),
            request.value(QStringLiteral("remark")).toString(),
            &errorMessage);
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), cmd},
                                {QStringLiteral("ok"), ok},
                                {QStringLiteral("message"), errorMessage},
                            });
        return;
    }

    if (cmd == QStringLiteral("delete_tunnel"))
    {
        const int tunnelId = request.value(QStringLiteral("id")).toInt();
        if (!tunnelBelongsToUser(tunnelId, userId))
        {
            sendResponse(socket, QJsonObject{
                                    {QStringLiteral("cmd"), cmd},
                                    {QStringLiteral("ok"), false},
                                    {QStringLiteral("message"), QStringLiteral("隧道不存在或不属于当前用户")},
                                });
            return;
        }
        const bool ok = m_DatabaseManager->deleteTunnel(tunnelId);
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), cmd},
                                {QStringLiteral("ok"), ok},
                                {QStringLiteral("message"), ok ? QString() : QStringLiteral("删除隧道失败")},
                            });
        return;
    }

    if (cmd == QStringLiteral("set_tunnel_enabled"))
    {
        const int tunnelId = request.value(QStringLiteral("id")).toInt();
        if (!tunnelBelongsToUser(tunnelId, userId))
        {
            sendResponse(socket, QJsonObject{
                                    {QStringLiteral("cmd"), cmd},
                                    {QStringLiteral("ok"), false},
                                    {QStringLiteral("message"), QStringLiteral("隧道不存在或不属于当前用户")},
                                });
            return;
        }
        const bool ok = m_DatabaseManager->setTunnelEnabled(
            tunnelId, request.value(QStringLiteral("enabled")).toBool());
        sendResponse(socket, QJsonObject{
                                {QStringLiteral("cmd"), cmd},
                                {QStringLiteral("ok"), ok},
                                {QStringLiteral("message"), ok ? QString() : QStringLiteral("更新隧道状态失败")},
                            });
        return;
    }

    sendResponse(socket, QJsonObject{
                            {QStringLiteral("cmd"), cmd},
                            {QStringLiteral("ok"), false},
                            {QStringLiteral("message"), QStringLiteral("未知命令: ") + cmd},
                        });
}

void PanelApiServer::sendResponse(QTcpSocket* socket, const QJsonObject& response)
{
    if (!socket)
    {
        return;
    }
    QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
    data.append('\n');
    socket->write(data);
}

QString PanelApiServer::issueToken(int userId)
{
    QString token;
    do
    {
        token = randomToken();
    } while (m_TokenToUserId.contains(token));
    m_TokenToUserId.insert(token, userId);
    return token;
}

int PanelApiServer::userIdByToken(const QString& token) const
{
    return m_TokenToUserId.value(token, -1);
}
