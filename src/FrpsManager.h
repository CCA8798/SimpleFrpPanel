#ifndef FRPSMANAGER_H
#define FRPSMANAGER_H

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class QProcess;

// frps 服务端进程管理：
// - 生成 frps.toml 配置（bindPort / auth.token / allowPorts 端口范围白名单）
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

    // 生成 frps.toml；portRanges 为各用户界定的远端端口范围 (min, max)，写入 allowPorts 白名单
    static bool generateConfig(const QString& configPath, quint16 bindPort,
                               const QString& token,
                               const QList<QPair<quint16, quint16>>& portRanges,
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
