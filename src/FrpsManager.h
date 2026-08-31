#ifndef FRPSMANAGER_H
#define FRPSMANAGER_H

#include <QList>
#include <QObject>
#include <QString>

class QProcess;

// frps 服务端进程管理：
// - 生成 frps.toml 配置（bindPort / auth.token / allowPorts 端口白名单）
// - 启动 / 停止 frps 进程，实时转发进程输出到日志信号
// - frps.exe 路径持久化在程序目录 config.ini
class FrpsManager : public QObject
{
    Q_OBJECT

public:
    explicit FrpsManager(QObject* parent = nullptr);
    ~FrpsManager() override;

    bool isRunning() const;
    QString frpsPath() const;
    void setFrpsPath(const QString& path);

    // 生成 frps.toml；allowedPorts 为已登记隧道的远端端口（白名单）
    static bool generateConfig(const QString& configPath, quint16 bindPort,
                               const QString& token, const QList<quint16>& allowedPorts,
                               QString* errorMessage = nullptr);

    bool start(const QString& configPath, QString* errorMessage = nullptr);
    void stop();

Q_SIGNALS:
    void runningChanged(bool running);
    void logMessage(const QString& text);

private:
    void readProcessOutput();

    QProcess* m_Process = nullptr;
    QString m_FrpsPath;
};

#endif // FRPSMANAGER_H
