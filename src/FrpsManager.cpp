#include "FrpsManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

namespace {
const QString kSettingsPathKey = QStringLiteral("frps/path");

QString appSettingsPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini");
}
} // namespace

FrpsManager::FrpsManager(const QString& settingsKey, QObject* parent)
    : QObject(parent)
    , m_SettingsKey(settingsKey.isEmpty() ? kSettingsPathKey : settingsKey)
{
    m_Process = new QProcess(this);
    m_Process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_Process, &QProcess::readyReadStandardOutput,
            this, &FrpsManager::readProcessOutput);
    connect(m_Process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                emit runningChanged(false);
            });

    QSettings settings(appSettingsPath(), QSettings::IniFormat);
    m_FrpsPath = settings.value(m_SettingsKey).toString();
}

FrpsManager::~FrpsManager()
{
    stop();
}

bool FrpsManager::isRunning() const
{
    return m_Process->state() != QProcess::NotRunning;
}

QString FrpsManager::frpsPath() const
{
    return m_FrpsPath;
}

void FrpsManager::setFrpsPath(const QString& path)
{
    m_FrpsPath = path;
    QSettings settings(appSettingsPath(), QSettings::IniFormat);
    settings.setValue(m_SettingsKey, path);
}

bool FrpsManager::generateConfig(const QString& configPath, quint16 bindPort,
                                 const QString& token,
                                 const QList<QPair<quint16, quint16>>& portRanges,
                                 quint16 webServerPort, const QString& webServerUser,
                                 const QString& webServerPassword,
                                 QString* errorMessage)
{
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法写入配置文件: %1").arg(configPath);
        }
        return false;
    }
    QTextStream stream(&file);
    // 显式使用 UTF-8：默认本地编码（中文系统为 GBK）会让 frp 的 TOML 解析报错
    stream.setCodec("UTF-8");
    // 注释使用纯 ASCII，避免任何解析器对非 ASCII 字符的兼容问题
    stream << QStringLiteral("# SimpleFrpPanel generated frps config\n");
    stream << QStringLiteral("bindPort = %1\n").arg(bindPort);
    stream << QStringLiteral("auth.token = \"%1\"\n").arg(token);
    if (webServerPort > 0)
    {
        // 仪表盘：流量监控数据源（TrafficMonitor 通过该 API 采样）
        stream << QStringLiteral("\nwebServer.addr = \"0.0.0.0\"\n");
        stream << QStringLiteral("webServer.port = %1\n").arg(webServerPort);
        stream << QStringLiteral("webServer.user = \"%1\"\n").arg(webServerUser);
        stream << QStringLiteral("webServer.password = \"%1\"\n").arg(webServerPassword);
    }
    if (!portRanges.isEmpty())
    {
        stream << QStringLiteral("\n# allowed remote port ranges of registered users\n");
        stream << QStringLiteral("allowPorts = [\n");
        for (int i = 0; i < portRanges.size(); ++i)
        {
            stream << QStringLiteral("  { start = %1, end = %2 }")
                          .arg(portRanges[i].first)
                          .arg(portRanges[i].second);
            stream << (i < portRanges.size() - 1 ? QStringLiteral(",") : QString()) << QStringLiteral("\n");
        }
        stream << QStringLiteral("]\n");
    }
    stream.flush();
    file.close();
    return true;
}

bool FrpsManager::generateFrpcConfig(const QString& configPath, const QString& serverAddr,
                                     quint16 serverPort, const QString& token,
                                     const QJsonArray& tunnels, QString* errorMessage)
{
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("无法写入配置文件: %1").arg(configPath);
        }
        return false;
    }
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << QStringLiteral("# SimpleFrpPanel generated frpc config\n");
    stream << QStringLiteral("serverAddr = \"%1\"\n").arg(serverAddr);
    stream << QStringLiteral("serverPort = %1\n").arg(serverPort);
    stream << QStringLiteral("auth.token = \"%1\"\n").arg(token);

    int proxyCount = 0;
    for (const QJsonValue& value : tunnels)
    {
        const QJsonObject tunnel = value.toObject();
        if (!tunnel.value(QStringLiteral("enabled")).toBool())
        {
            continue;
        }
        const QString protocol = tunnel.value(QStringLiteral("protocol")).toString();
        if (protocol != QStringLiteral("tcp") && protocol != QStringLiteral("udp")
            && protocol != QStringLiteral("http") && protocol != QStringLiteral("https"))
        {
            continue;
        }
        stream << QStringLiteral("\n[[proxies]]\n");
        stream << QStringLiteral("name = \"%1\"\n").arg(tunnel.value(QStringLiteral("name")).toString());
        stream << QStringLiteral("type = \"%1\"\n").arg(protocol);
        stream << QStringLiteral("localIP = \"%1\"\n").arg(tunnel.value(QStringLiteral("localIp")).toString());
        stream << QStringLiteral("localPort = %1\n").arg(tunnel.value(QStringLiteral("localPort")).toInt());
        if (protocol == QStringLiteral("tcp") || protocol == QStringLiteral("udp"))
        {
            stream << QStringLiteral("remotePort = %1\n").arg(tunnel.value(QStringLiteral("remotePort")).toInt());
        }
        else
        {
            stream << QStringLiteral("customDomains = [\"%1\"]\n")
                          .arg(tunnel.value(QStringLiteral("customDomain")).toString());
        }
        ++proxyCount;
    }

    stream.flush();
    file.close();
    if (proxyCount == 0 && errorMessage)
    {
        *errorMessage = QStringLiteral("没有启用的隧道，无法生成 frpc 配置");
    }
    return proxyCount > 0;
}

bool FrpsManager::start(const QString& configPath, QString* errorMessage)
{
    if (isRunning())
    {
        return true;
    }
    if (m_FrpsPath.trimmed().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("尚未设置 frps.exe 路径");
        }
        return false;
    }
    if (!QFile::exists(m_FrpsPath))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("frps.exe 不存在: %1").arg(m_FrpsPath);
        }
        return false;
    }

    m_Process->setProgram(m_FrpsPath);
    m_Process->setArguments(QStringList() << QStringLiteral("-c") << configPath);
    m_Process->start();
    if (!m_Process->waitForStarted(5000))
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("frps 启动失败: %1").arg(m_Process->errorString());
        }
        return false;
    }
    emit runningChanged(true);
    return true;
}

void FrpsManager::stop()
{
    if (m_Process->state() == QProcess::NotRunning)
    {
        return;
    }
    m_Process->terminate();
    if (!m_Process->waitForFinished(3000))
    {
        m_Process->kill();
        m_Process->waitForFinished(2000);
    }
    emit runningChanged(false);
}

void FrpsManager::readProcessOutput()
{
    const QString text = QString::fromUtf8(m_Process->readAllStandardOutput());
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]")),
                                         QString::SkipEmptyParts);
    for (const QString& line : lines)
    {
        emit logMessage(QStringLiteral("[%1] %2")
                            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                 line));
    }
}
