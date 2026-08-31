#include "TrafficMonitor.h"

#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "DatabaseManager.h"

namespace {
const QString kSettingWebPort = QStringLiteral("frps_web_port");
const QString kSettingWebUser = QStringLiteral("frps_web_user");
const QString kSettingWebPassword = QStringLiteral("frps_web_password");

QByteArray basicAuthHeader(const QString& user, const QString& password)
{
    const QByteArray credentials = (user + QLatin1Char(':') + password).toUtf8();
    return QByteArrayLiteral("Basic ") + credentials.toBase64();
}
} // namespace

TrafficMonitor::TrafficMonitor(DatabaseManager* databaseManager, QObject* parent)
    : QObject(parent)
    , m_DatabaseManager(databaseManager)
{
    m_Network = new QNetworkAccessManager(this);
    // 关键：默认会走系统代理，本机回环请求可能被代理劫持导致采样失败，必须直连
    m_Network->setProxy(QNetworkProxy::NoProxy);
    connect(m_Network, &QNetworkAccessManager::finished, this, &TrafficMonitor::onReplyFinished);

    m_Timer = new QTimer(this);
    m_Timer->setInterval(10000); // 每 10 秒采样一轮
    connect(m_Timer, &QTimer::timeout, this, &TrafficMonitor::poll);
    m_Timer->start();
}

void TrafficMonitor::poll()
{
    if (!m_DatabaseManager || !m_DatabaseManager->isOpen())
    {
        m_LastSamples.clear();
        return;
    }
    const QString webPort = m_DatabaseManager->getSetting(kSettingWebPort);
    const QString webUser = m_DatabaseManager->getSetting(kSettingWebUser);
    const QString webPassword = m_DatabaseManager->getSetting(kSettingWebPassword);
    if (webPort.trimmed().isEmpty() || webUser.trimmed().isEmpty() || webPassword.trimmed().isEmpty())
    {
        m_LastSamples.clear();
        return;
    }

    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        const QList<DatabaseManager::TunnelInfo> tunnels = m_DatabaseManager->queryTunnels(user.id);
        for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
        {
            if (!tunnel.isEnabled)
            {
                continue; // 禁用的隧道不产生流量
            }
            fetchTunnel(user.id, user.username, tunnel);
        }
    }
    // 清理已不存在隧道的基准（隧道被删除后不再采样）
    QList<int> activeTunnelIds;
    const QList<DatabaseManager::UserInfo> usersForCleanup = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : usersForCleanup)
    {
        const QList<DatabaseManager::TunnelInfo> tunnels = m_DatabaseManager->queryTunnels(user.id);
        for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
        {
            activeTunnelIds.append(tunnel.id);
        }
    }
    for (auto it = m_LastSamples.begin(); it != m_LastSamples.end();)
    {
        if (!activeTunnelIds.contains(it.key()))
        {
            it = m_LastSamples.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void TrafficMonitor::clearBaselines()
{
    m_LastSamples.clear();
    m_HasLoggedSuccess = false;
    m_LastErrorLogTime = 0;
}

void TrafficMonitor::fetchTunnel(int userId, const QString& userName,
                                 const DatabaseManager::TunnelInfo& tunnel)
{
    const QString webPort = m_DatabaseManager->getSetting(kSettingWebPort);
    const QString webUser = m_DatabaseManager->getSetting(kSettingWebUser);
    const QString webPassword = m_DatabaseManager->getSetting(kSettingWebPassword);

    // 代理类型与隧道协议一致（frpc.toml 的 proxies 由本面板生成，名称 = 隧道名）；
    // 显式构造 URL 并对隧道名做百分号编码，避免中文/特殊字符导致请求 404
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(QStringLiteral("127.0.0.1"));
    url.setPort(webPort.trimmed().toUShort());
    url.setPath(QStringLiteral("/api/proxy/%1/%2")
                    .arg(tunnel.protocol,
                         QString::fromLatin1(QUrl::toPercentEncoding(tunnel.name))));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", basicAuthHeader(webUser.trimmed(), webPassword.trimmed()));

    QNetworkReply* reply = m_Network->get(request);
    reply->setProperty("tunnelId", tunnel.id);
    reply->setProperty("tunnelName", tunnel.name);
    reply->setProperty("userId", userId);
    reply->setProperty("userName", userName);
}

void TrafficMonitor::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    const int tunnelId = reply->property("tunnelId").toInt();
    const QString tunnelName = reply->property("tunnelName").toString();
    const int userId = reply->property("userId").toInt();
    const QString userName = reply->property("userName").toString();

    if (reply->error() != QNetworkReply::NoError)
    {
        // frps 未运行 / webServer 未启用 / 代理不存在：跳过本轮
        m_LastSamples.remove(tunnelId);
        // 失败提示（每 60 秒最多一次，便于在服务端隧道页日志中诊断）
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_LastErrorLogTime > 60000)
        {
            m_LastErrorLogTime = now;
            emit logMessage(QStringLiteral("[%1] 流量采样失败（frps 未运行或仪表盘未启用/端口不符）: %2")
                                .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                     reply->errorString()));
        }
        return;
    }
    if (!m_HasLoggedSuccess)
    {
        m_HasLoggedSuccess = true;
        emit logMessage(QStringLiteral("[%1] 流量采样已连接 frps 仪表盘（隧道 %2）")
                            .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                 tunnelName));
    }
    const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
    const qint64 todayIn = object.value(QStringLiteral("todayTrafficIn")).toVariant().toLongLong();
    const qint64 todayOut = object.value(QStringLiteral("todayTrafficOut")).toVariant().toLongLong();

    Sample current{todayIn, todayOut};
    Sample previous = m_LastSamples.value(tunnelId);
    if (m_LastSamples.contains(tunnelId))
    {
        qint64 deltaIn = current.bytesIn - previous.bytesIn;
        qint64 deltaOut = current.bytesOut - previous.bytesOut;
        // frps 重启/代理重连会使计数器归零，负增量钳制为 0
        if (deltaIn < 0)
        {
            deltaIn = 0;
        }
        if (deltaOut < 0)
        {
            deltaOut = 0;
        }
        m_DatabaseManager->addTraffic(userId, userName, tunnelId, tunnelName,
                                      QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")),
                                      deltaIn, deltaOut);
    }
    m_LastSamples.insert(tunnelId, current);
}
