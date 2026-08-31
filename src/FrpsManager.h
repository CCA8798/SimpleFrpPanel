#ifndef FRPSMANAGER_H
#define FRPSMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class QProcess;

// frp 进程管理（frps / frpc 通用）：
// - frps：生成 frps.toml（bindPort / auth.token / allowPorts 端口范围白名单）
// - frpc：生成 frpc.toml（serverAddr / serverPort / auth.token / proxies，由客户端隧道生成）
// - 启动 / 停止进程，实时转发进程输出到日志信号
// - 程序路径持久化在程序目录 config.ini（settingsKey 区分 frps 与 frpc）
class FrpsManager : public QObject
{
    Q_OBJECT

public:
    explicit FrpsManager(const QString& settingsKey = QStringLiteral("frps/path"),
                         QObject* parent = nullptr);
    ~FrpsManager() override;

    bool isRunning() const;
    QString frpsPath() const;
    void setFrpsPath(const QString& path);

    // 生成 frps.toml；portRanges 为各用户界定的远端端口范围 (min, max)，写入 allowPorts 白名单；
    // webServerPort > 0 时启用仪表盘（流量监控数据源），user/password 为 Basic 认证凭据
    static bool generateConfig(const QString& configPath, quint16 bindPort,
                               const QString& token,
                               const QList<QPair<quint16, quint16>>& portRanges,
                               quint16 webServerPort, const QString& webServerUser,
                               const QString& webServerPassword,
                               QString* errorMessage = nullptr);

    // 生成 frpc.toml；tunnels 为当前用户启用的隧道（enabled == true 的条目）
    static bool generateFrpcConfig(const QString& configPath, const QString& serverAddr,
                                   quint16 serverPort, const QString& token,
                                   const QJsonArray& tunnels, QString* errorMessage = nullptr);

    bool start(const QString& configPath, QString* errorMessage = nullptr);
    void stop();

Q_SIGNALS:
    void runningChanged(bool running);
    void logMessage(const QString& text);

private:
    void readProcessOutput();

    QProcess* m_Process = nullptr;
    QString m_FrpsPath;
    QString m_SettingsKey;
};

#endif // FRPSMANAGER_H
