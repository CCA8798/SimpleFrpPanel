#ifndef TRAFFICMONITOR_H
#define TRAFFICMONITOR_H

#include <QHash>
#include <QObject>

#include "DatabaseManager.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

// 流量监控器：定期轮询 frps 仪表盘 API（webServer），把每个启用隧道的今日流量增量
// 累加写入 traffic_records 表（按 用户+隧道+日期）。
// 数据源：GET /api/proxy/{type}/{name} → { todayTrafficIn, todayTrafficOut }（Basic 认证）
class TrafficMonitor : public QObject
{
    Q_OBJECT

public:
    explicit TrafficMonitor(DatabaseManager* databaseManager, QObject* parent = nullptr);

    void poll();           // 立即采样一轮
    void clearBaselines(); // 数据库切换/隧道变化时重置采样基准

private:
    struct Sample
    {
        qint64 bytesIn = 0;
        qint64 bytesOut = 0;
    };

    void fetchTunnel(int userId, const QString& userName, const DatabaseManager::TunnelInfo& tunnel);
    void onReplyFinished(QNetworkReply* reply);

    DatabaseManager* m_DatabaseManager = nullptr;
    QNetworkAccessManager* m_Network = nullptr;
    QTimer* m_Timer = nullptr;
    // tunnelId -> 上次采样值（用于计算增量；frps 重启/计数器归零时增量钳制为 0）
    QHash<int, Sample> m_LastSamples;
};

#endif // TRAFFICMONITOR_H
