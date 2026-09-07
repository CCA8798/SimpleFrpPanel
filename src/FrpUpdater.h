#ifndef FRPUPDATER_H
#define FRPUPDATER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// frp 程序（frpc.exe / frps.exe）的目录约定与更新检查：
// - 两个程序约定放在 <程序运行目录>/frp/ 下（发布包内置，或由用户放置）；
// - 每次程序启动时检查 GitHub (fatedier/frp) 最新版本，比内置版本新时提示用户更新；
// - 更新采用"确认后下载 windows_amd64 压缩包 -> 解压 -> 覆盖 frp/ 下的 exe"流程。
class FrpUpdater : public QObject
{
    Q_OBJECT

public:
    explicit FrpUpdater(QObject* parent = nullptr);

    // 目录/路径约定
    static QString frpDirectory();  // <程序运行目录>/frp
    static QString frpcPath();      // <程序运行目录>/frp/frpc.exe
    static QString frpsPath();      // <程序运行目录>/frp/frps.exe
    // 内置程序版本（读取 frp/frp_version.txt；缺文件时返回空）
    static QString bundledVersion();

    // 异步检查更新（联网查询 GitHub 最新 release）
    void checkForUpdates();

    // 尝试下载最新版并安装到 frp/（用于 frp 缺失时"尝试下载"：
    // 内部先联网获取最新发布信息，成功后自动下载）
    void downloadLatest();

    // 异步执行更新（下载并替换 frp/ 下程序）
    void performUpdate(const QString& downloadUrl);

Q_SIGNALS:
    // 检查完成：latestVersion 为空 = 网络失败或已是最新（message 说明原因）
    void checkFinished(const QString& currentVersion, const QString& latestVersion,
                       const QString& downloadUrl, const QString& message);
    // 下载进度（字节）
    void downloadProgress(qint64 received, qint64 total);
    // 更新过程中的提示消息（可用于日志）
    void updateProgress(const QString& message);
    // 更新结束：success=false 时 message 为失败原因
    void updateFinished(bool success, const QString& message);

private slots:
    void onLatestReleaseReply(QNetworkReply* reply);
    void onDownloadReply(QNetworkReply* reply);

private:
    void fetchLatestRelease(bool autoDownload);
    void applyDownloadUrl(const QString& url);
    // 将 <appDir>/frp/ 下文件加入"更新中"保护：覆盖前检查 exe 是否被占用
    static bool tryReplaceExe(const QString& targetPath, const QString& sourcePath,
                              QString* errorMessage);
    // "v0.61.1" 形式版本号比较：a>b 返回正、相等 0、a<b 负
    static int compareVersion(const QString& a, const QString& b);
    // 尝试从内置 exe 读取版本（运行 frps.exe -v），失败返回空
    static QString detectVersionFromExecutable();

    QNetworkAccessManager* m_Network = nullptr;
    QString m_LastDownloadUrl;
    QString m_TempZipPath;
    QString m_TempExtractDir;
    bool m_AutoDownloadAfterFetch = false;
};

#endif // FRPUPDATER_H
