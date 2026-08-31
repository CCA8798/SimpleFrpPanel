#include "PanelClient.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>

namespace {
QByteArray buildLine(const QJsonObject& object)
{
    QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    data.append('\n');
    return data;
}
} // namespace

PanelClient::PanelClient(QObject* parent)
    : QObject(parent)
{
    m_Socket = new QTcpSocket(this);
    m_TimeoutTimer = new QTimer(this);
    m_TimeoutTimer->setSingleShot(true);
    m_TimeoutTimer->setInterval(5000);

    connect(m_Socket, &QTcpSocket::connected, this, &PanelClient::onConnected);
    connect(m_Socket, &QTcpSocket::disconnected, this, &PanelClient::onDisconnected);
    connect(m_Socket, &QTcpSocket::readyRead, this, &PanelClient::onReadyRead);
    connect(m_Socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, [this](QAbstractSocket::SocketError) {
                if (m_WaitingLoginResponse)
                {
                    m_WaitingLoginResponse = false;
                    emit loginFailed(QStringLiteral("无法连接服务器: %1").arg(m_Socket->errorString()));
                }
                emit logMessage(QStringLiteral("[%1] 连接错误: %2")
                                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                         m_Socket->errorString()));
            });
    connect(m_TimeoutTimer, &QTimer::timeout, this, &PanelClient::onConnectionTimeout);
}

bool PanelClient::isConnected() const
{
    return m_Socket->state() == QAbstractSocket::ConnectedState;
}

bool PanelClient::isLoggedIn() const
{
    return !m_Token.isEmpty();
}

QString PanelClient::username() const
{
    return m_Username;
}

void PanelClient::connectToServer(const QString& host, quint16 port)
{
    if (isConnected())
    {
        disconnectFromServer();
    }
    m_TimeoutTimer->start();
    m_Socket->connectToHost(host, port);
}

void PanelClient::disconnectFromServer()
{
    m_TimeoutTimer->stop();
    m_Token.clear();
    m_Username.clear();
    m_WaitingLoginResponse = false;
    if (m_Socket->state() != QAbstractSocket::UnconnectedState)
    {
        m_Socket->abort();
    }
    emit connectionStateChanged(false);
}

void PanelClient::login(const QString& username, const QString& password)
{
    m_WaitingLoginResponse = true;
    m_TimeoutTimer->start();
    sendRequest(QJsonObject{
        {QStringLiteral("cmd"), QStringLiteral("login")},
        {QStringLiteral("username"), username},
        {QStringLiteral("password"), password},
    });
}

void PanelClient::logout()
{
    if (isLoggedIn())
    {
        sendRequest(QJsonObject{
            {QStringLiteral("cmd"), QStringLiteral("logout")},
            {QStringLiteral("token"), m_Token},
        });
    }
    m_Token.clear();
    m_Username.clear();
    emit loggedOut();
}

void PanelClient::requestTunnels()
{
    if (!isLoggedIn())
    {
        return;
    }
    sendRequest(QJsonObject{
        {QStringLiteral("cmd"), QStringLiteral("list")},
        {QStringLiteral("token"), m_Token},
    });
}

void PanelClient::addTunnel(const QString& name, const QString& protocol, int remotePort,
                            const QString& localIp, int localPort, const QString& customDomain,
                            bool enabled, const QString& remark)
{
    sendRequest(QJsonObject{
        {QStringLiteral("cmd"), QStringLiteral("add_tunnel")},
        {QStringLiteral("token"), m_Token},
        {QStringLiteral("name"), name},
        {QStringLiteral("protocol"), protocol},
        {QStringLiteral("remotePort"), remotePort},
        {QStringLiteral("localIp"), localIp},
        {QStringLiteral("localPort"), localPort},
        {QStringLiteral("customDomain"), customDomain},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("remark"), remark},
    });
}

void PanelClient::updateTunnel(int id, const QString& name, const QString& protocol, int remotePort,
                               const QString& localIp, int localPort, const QString& customDomain,
                               bool enabled, const QString& remark)
{
    sendRequest(QJsonObject{
        {QStringLiteral("cmd"), QStringLiteral("update_tunnel")},
        {QStringLiteral("token"), m_Token},
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("protocol"), protocol},
        {QStringLiteral("remotePort"), remotePort},
        {QStringLiteral("localIp"), localIp},
        {QStringLiteral("localPort"), localPort},
        {QStringLiteral("customDomain"), customDomain},
        {QStringLiteral("enabled"), enabled},
        {QStringLiteral("remark"), remark},
    });
}

void PanelClient::deleteTunnel(int id)
{
    sendRequest(QJsonObject{
        {QStringLiteral("cmd"), QStringLiteral("delete_tunnel")},
        {QStringLiteral("token"), m_Token},
        {QStringLiteral("id"), id},
    });
}

void PanelClient::setTunnelEnabled(int id, bool enabled)
{
    sendRequest(QJsonObject{
        {QStringLiteral("cmd"), QStringLiteral("set_tunnel_enabled")},
        {QStringLiteral("token"), m_Token},
        {QStringLiteral("id"), id},
        {QStringLiteral("enabled"), enabled},
    });
}

void PanelClient::sendRequest(const QJsonObject& request)
{
    QJsonObject object = request;
    object.insert(QStringLiteral("seq"), ++m_RequestSeq);
    m_Socket->write(buildLine(object));
}

void PanelClient::onConnected()
{
    m_TimeoutTimer->stop();
    emit connectionStateChanged(true);
    emit logMessage(QStringLiteral("[%1] 已连接到服务器 %2:%3")
                        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                             m_Socket->peerAddress().toString())
                        .arg(m_Socket->peerPort()));
}

void PanelClient::onDisconnected()
{
    m_TimeoutTimer->stop();
    const bool hadToken = !m_Token.isEmpty();
    m_Token.clear();
    m_Username.clear();
    m_WaitingLoginResponse = false;
    emit connectionStateChanged(false);
    if (hadToken)
    {
        emit loggedOut();
    }
}

void PanelClient::onConnectionTimeout()
{
    if (m_WaitingLoginResponse)
    {
        m_WaitingLoginResponse = false;
        emit loginFailed(QStringLiteral("连接或登录超时"));
    }
    m_Socket->abort();
}

void PanelClient::onReadyRead()
{
    m_ReceiveBuffer.append(m_Socket->readAll());
    while (true)
    {
        const int newlineIndex = m_ReceiveBuffer.indexOf('\n');
        if (newlineIndex < 0)
        {
            break;
        }
        const QByteArray line = m_ReceiveBuffer.left(newlineIndex).trimmed();
        m_ReceiveBuffer.remove(0, newlineIndex + 1);
        if (line.isEmpty())
        {
            continue;
        }
        const QJsonDocument document = QJsonDocument::fromJson(line);
        if (!document.isObject())
        {
            continue;
        }
        const QJsonObject response = document.object();
        const QString cmd = response.value(QStringLiteral("cmd")).toString();

        if (cmd == QStringLiteral("login"))
        {
            m_TimeoutTimer->stop();
            m_WaitingLoginResponse = false;
            if (response.value(QStringLiteral("ok")).toBool())
            {
                m_Token = response.value(QStringLiteral("token")).toString();
                m_Username = response.value(QStringLiteral("username")).toString();
                emit loginSucceeded(response.value(QStringLiteral("quota")).toObject(),
                                    response.value(QStringLiteral("serverInfo")).toObject());
                emit logMessage(QStringLiteral("[%1] 登录成功: %2")
                                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                         m_Username));
            }
            else
            {
                emit loginFailed(response.value(QStringLiteral("message")).toString());
                emit logMessage(QStringLiteral("[%1] 登录失败: %2")
                                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                         response.value(QStringLiteral("message")).toString()));
            }
            continue;
        }

        if (cmd == QStringLiteral("logout"))
        {
            m_Token.clear();
            m_Username.clear();
            emit loggedOut();
            continue;
        }

        if (cmd == QStringLiteral("list"))
        {
            if (response.value(QStringLiteral("ok")).toBool())
            {
                emit tunnelsReceived(response.value(QStringLiteral("tunnels")).toArray(),
                                     response.value(QStringLiteral("quota")).toObject(),
                                     response.value(QStringLiteral("frpsRunning")).toBool());
            }
            else
            {
                emit commandFailed(cmd, response.value(QStringLiteral("message")).toString());
            }
            continue;
        }

        // 增删改/开关等命令
        if (response.value(QStringLiteral("ok")).toBool())
        {
            emit commandSucceeded(cmd);
        }
        else
        {
            emit commandFailed(cmd, response.value(QStringLiteral("message")).toString());
        }
    }
}
