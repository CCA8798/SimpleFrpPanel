#include "FrpUpdater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include "Information.h"

namespace {
const QString kVersionFileName = QStringLiteral("frp_version.txt");
const QString kTempZipName = QStringLiteral("frp_update.zip");
const QString kTempExtractName = QStringLiteral("frp_update_extract");
const QString kGithubReleasesApi =
    QStringLiteral("https://api.github.com/repos/fatedier/frp/releases/latest");

QString frpTr(const char* text)
{
    return QCoreApplication::translate("SimpleFrpPanel", text);
}

// 在 <targetDir> 下查找目标可执行文件（递归一层子目录，兼容压缩包内 frp_x_windows_amd64/ 结构）
QString findExecutableUnder(const QString& dir, const QString& exeName)
{
    const QString direct = dir + QLatin1Char('/') + exeName;
    if (QFile::exists(direct))
    {
        return direct;
    }
    const QDir root(dir);
    for (const QString& sub : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        const QString candidate = root.filePath(sub + QLatin1Char('/') + exeName);
        if (QFile::exists(candidate))
        {
            return candidate;
        }
    }
    return QString();
}

QString downloadFileNameFromUrl(const QString& url)
{
    const int slash = url.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? url.mid(slash + 1) : QString();
}
} // namespace

FrpUpdater::FrpUpdater(QObject* parent)
    : QObject(parent)
{
    m_Network = new QNetworkAccessManager(this);
    // 更新代理：设置页可配 HTTP 代理（如 127.0.0.1:7897）；留空时使用系统代理
    const QString proxySetting = g_GlobalInformation.updateProxy.trimmed();
    if (!proxySetting.isEmpty())
    {
        QString host = proxySetting;
        int port = 0;
        const int colon = proxySetting.indexOf(QLatin1Char(':'));
        if (colon > 0)
        {
            host = proxySetting.left(colon);
            port = proxySetting.mid(colon + 1).toInt();
        }
        if (!host.isEmpty() && port > 0 && port < 65536)
        {
            m_Network->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, host,
                                              static_cast<quint16>(port)));
        }
    }
}

QString FrpUpdater::frpDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/frp");
}

QString FrpUpdater::frpcPath()
{
    return frpDirectory() + QStringLiteral("/frpc.exe");
}

QString FrpUpdater::frpsPath()
{
    return frpDirectory() + QStringLiteral("/frps.exe");
}

QString FrpUpdater::bundledVersion()
{
    QFile file(frpDirectory() + QLatin1Char('/') + kVersionFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

int FrpUpdater::compareVersion(const QString& a, const QString& b)
{
    const QString cleanA = a.trimmed();
    const QString cleanB = b.trimmed();
    // 去掉常见前缀 v/V
    const QStringList partsA = cleanA.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)
                                   ? cleanA.mid(1).split(QLatin1Char('.'))
                                   : cleanA.split(QLatin1Char('.'));
    const QStringList partsB = cleanB.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)
                                   ? cleanB.mid(1).split(QLatin1Char('.'))
                                   : cleanB.split(QLatin1Char('.'));
    const int count = qMax(partsA.size(), partsB.size());
    for (int i = 0; i < count; ++i)
    {
        const int numA = (i < partsA.size()) ? partsA[i].toInt() : 0;
        const int numB = (i < partsB.size()) ? partsB[i].toInt() : 0;
        if (numA != numB)
        {
            return numA > numB ? 1 : -1;
        }
    }
    return 0;
}

QString FrpUpdater::detectVersionFromExecutable()
{
    // frps/frpc 支持 -v，输出形如 "frp 0.61.1"
    for (const QString& exePath : {frpsPath(), frpcPath()})
    {
        if (!QFile::exists(exePath))
        {
            continue;
        }
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(exePath, {QStringLiteral("-v")});
        if (!process.waitForStarted(3000))
        {
            continue;
        }
        if (!process.waitForFinished(5000))
        {
            process.kill();
            process.waitForFinished(1000);
            continue;
        }
        const QString output = QString::fromUtf8(process.readAll());
        const QRegularExpressionMatch match =
            QRegularExpression(QStringLiteral("frp\\s+v?([0-9]+\\.[0-9]+\\.[0-9]+)"),
                               QRegularExpression::CaseInsensitiveOption)
                .match(output);
        if (match.hasMatch())
        {
            return match.captured(1);
        }
    }
    return QString();
}

void FrpUpdater::checkForUpdates()
{
    m_AutoDownloadAfterFetch = false;
    fetchLatestRelease(false);
}

void FrpUpdater::downloadLatest()
{
    // frp 缺失时"尝试下载"：先联网获取最新发布信息，成功后自动下载
    m_AutoDownloadAfterFetch = true;
    fetchLatestRelease(true);
}

void FrpUpdater::fetchLatestRelease(bool autoDownload)
{
    Q_UNUSED(autoDownload)
    emit updateProgress(frpTr("正在获取 frp 最新版本信息…"));
    QNetworkRequest request{QUrl(kGithubReleasesApi)};
    request.setRawHeader("User-Agent", "SimpleFrpPanel");
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply* reply = m_Network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onLatestReleaseReply(reply); });
}

void FrpUpdater::onLatestReleaseReply(QNetworkReply* reply)
{
    reply->deleteLater();
    const bool autoDownload = m_AutoDownloadAfterFetch;
    const QString failMessage =
        frpTr("无法获取 frp 下载信息（请检查网络或更新代理）");

    if (reply->error() != QNetworkReply::NoError)
    {
        if (autoDownload)
        {
            emit updateFinished(false, failMessage);
        }
        else
        {
            // 内置版本（版本文件优先；无则尝试从 exe 读取）
            QString current = bundledVersion();
            if (current.isEmpty())
            {
                current = detectVersionFromExecutable();
            }
            emit checkFinished(current, QString(), QString(), failMessage);
        }
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = root.value(QStringLiteral("tag_name")).toString(); // 如 v0.71.0
    QString downloadUrl;
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& value : assets)
    {
        const QString name = value.toObject().value(QStringLiteral("name")).toString();
        if (name.contains(QStringLiteral("windows_amd64")) && name.endsWith(QStringLiteral(".zip")))
        {
            downloadUrl = value.toObject().value(QStringLiteral("browser_download_url")).toString();
            break;
        }
    }
    QString latest = tag;
    if (latest.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
    {
        latest = latest.mid(1);
    }

    // 自动下载模式：直接进入下载
    if (autoDownload)
    {
        if (downloadUrl.isEmpty())
        {
            emit updateFinished(false, failMessage);
            return;
        }
        performUpdate(downloadUrl);
        return;
    }

    // 普通检查模式
    QString current = bundledVersion();
    if (current.isEmpty())
    {
        current = detectVersionFromExecutable();
    }
    if (current.isEmpty())
    {
        // frp/ 缺失：交给 UI（latest 有值时说明可下载）
        emit checkFinished(QString(), latest, downloadUrl,
                           frpTr("frp 未内置（缺少 frp/frpc.exe）"));
        return;
    }
    if (downloadUrl.isEmpty() || compareVersion(latest, current) <= 0)
    {
        emit checkFinished(current, QString(), QString(),
                           frpTr("已是最新版本（frp %1）").arg(current));
        return;
    }
    emit checkFinished(current, latest, downloadUrl,
                       frpTr("发现新版本 frp %1").arg(latest));
}

void FrpUpdater::performUpdate(const QString& downloadUrl)
{
    m_LastDownloadUrl = downloadUrl;
    const QDir dir(frpDirectory());
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
    {
        emit updateFinished(false, frpTr("无法创建目录 %1").arg(frpDirectory()));
        return;
    }
    const QString fileName = downloadFileNameFromUrl(downloadUrl);
    m_TempZipPath = dir.filePath(fileName.isEmpty() ? kTempZipName : fileName);
    m_TempExtractDir = dir.filePath(kTempExtractName);
    QDir(dir).mkpath(kTempExtractName);

    emit updateProgress(frpTr("正在下载 frp 更新包…"));
    QNetworkRequest request{QUrl(downloadUrl)};
    request.setRawHeader("User-Agent", "SimpleFrpPanel");
    QNetworkReply* reply = m_Network->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit downloadProgress(received, total); });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onDownloadReply(reply); });
}

void FrpUpdater::onDownloadReply(QNetworkReply* reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        emit updateFinished(false, frpTr("下载失败: %1").arg(reply->errorString()));
        return;
    }
    QFile zipFile(m_TempZipPath);
    if (!zipFile.open(QIODevice::WriteOnly))
    {
        emit updateFinished(false, frpTr("无法写入文件 %1").arg(m_TempZipPath));
        return;
    }
    zipFile.write(reply->readAll());
    zipFile.close();

    emit updateProgress(frpTr("正在解压更新包…"));
    // 解压：Windows 10+ 自带 tar.exe（bsdtar 支持 zip）
    const QDir dir(frpDirectory());
    const QString extractDir = m_TempExtractDir;
    QProcess tar;
    tar.setProcessChannelMode(QProcess::MergedChannels);
    tar.start(QStringLiteral("tar"), {QStringLiteral("-xf"), m_TempZipPath,
                                      QStringLiteral("-C"), extractDir});
    if (!tar.waitForStarted(3000) || !tar.waitForFinished(30000))
    {
        emit updateFinished(false, frpTr("解压失败（需要 Windows 10+ 自带 tar）"));
        return;
    }

    // 提取并覆盖
    QString errorMessage;
    const bool ok = tryReplaceExe(frpsPath(),
                                  findExecutableUnder(extractDir, QStringLiteral("frps.exe")),
                                  &errorMessage)
                    && tryReplaceExe(frpcPath(),
                                     findExecutableUnder(extractDir, QStringLiteral("frpc.exe")),
                                     &errorMessage);
    if (ok)
    {
        // 记录新版本号到版本文件
        const QString latestTag = m_LastDownloadUrl;
        // 从下载 URL 推断版本号（.../download/vX.Y.Z/frp_...）
        QRegularExpression re(QStringLiteral("/download/v?([0-9]+\\.[0-9]+\\.[0-9]+)/"),
                              QRegularExpression::CaseInsensitiveOption);
        QString version;
        const QRegularExpressionMatch match = re.match(latestTag);
        if (match.hasMatch())
        {
            version = match.captured(1);
        }
        QFile versionFile(dir.filePath(kVersionFileName));
        if (versionFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            versionFile.write(version.toUtf8());
            versionFile.close();
        }
        emit updateProgress(frpTr("frp 已更新到 v%1").arg(version));
        emit updateFinished(true, frpTr("frp 已更新到 v%1").arg(version));
    }
    else
    {
        emit updateFinished(false,
                            errorMessage.isEmpty() ? frpTr("更新失败：压缩包内未找到 frp 程序")
                                                   : errorMessage);
    }
    // 清理临时文件
    QFile::remove(m_TempZipPath);
    QDir(extractDir).removeRecursively();
}

bool FrpUpdater::tryReplaceExe(const QString& targetPath, const QString& sourcePath,
                               QString* errorMessage)
{
    if (sourcePath.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = frpTr("更新包中缺少 %1").arg(QFileInfo(targetPath).fileName());
        }
        return false;
    }
    QFile::remove(targetPath); // 覆盖前删除（若被占用 remove 失败）
    if (!QFile::copy(sourcePath, targetPath))
    {
        if (errorMessage)
        {
            *errorMessage = frpTr("无法覆盖 %1（可能正在运行，请先停止）")
                                .arg(QFileInfo(targetPath).fileName());
        }
        return false;
    }
    return true;
}
