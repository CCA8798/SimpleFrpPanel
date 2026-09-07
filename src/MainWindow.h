#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

class QCloseEvent;
class QDialog;
class QMenu;
class QProgressBar;
class QResizeEvent;
class QSystemTrayIcon;
class ElaText;
class FrpUpdater;
class HomePage;
class ServerTunnelPage;
class ServerUserPage;
class TrafficPage;
class ClientTunnelPage;

// 主窗口：使用 Ela 的 ElaWindow（无边框 + 导航栏 + 自绘标题栏）。
// 左侧导航结构：
//   首页
//   【服务端】隧道管理 / 用户管理 / 流量统计
//   【客户端】隧道管理
// 导航栏宽度固定为窗口宽度的 30%。
// 标题栏右上角：设置按钮（齿轮，紧邻日/夜主题切换按钮左侧）。
// 关闭行为（由设置控制）：
//   - 默认每次点关闭弹出询问：「后台运行」/「直接退出」，并同步询问下次是否继续弹出；
//   - 不再询问时按记忆动作执行（config.ini，设置按钮可重新开启）；
//   - 系统托盘右键菜单"退出"始终直接退出。
class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupTrayIcon();
    void hideToTray();
    // 按设置处理一次关闭请求：询问 / 按记忆动作直接执行
    void handleCloseRequest();
    // 弹出「后台运行 / 直接退出」询问（含"下次继续弹出"勾选项）
    void showClosePrompt();
    // frp 更新检查完成回调：frp 缺失时询问是否下载；有新版本时询问是否更新
    void onFrpCheckFinished(const QString& currentVersion, const QString& latestVersion,
                            const QString& downloadUrl, const QString& message);
    // 通用的"确认并执行 frp 下载/更新"对话框
    void showFrpConfirmDialog(const QString& titleText, const QString& bodyText,
                              const QString& actionButtonText, const QString& downloadUrl,
                              const QString& actionBarTitle);
    // frp 下载/更新进度对话框
    void showFrpProgress(const QString& initialText);
    void setFrpProgressValue(qint64 received, qint64 total);
    void setFrpProgressText(const QString& text);
    void closeFrpProgress();

    HomePage* m_HomePage = nullptr;
    ServerTunnelPage* m_ServerTunnelPage = nullptr;
    ServerUserPage* m_ServerUserPage = nullptr;
    TrafficPage* m_TrafficPage = nullptr;
    ClientTunnelPage* m_ClientTunnelPage = nullptr;
    FrpUpdater* m_FrpUpdater = nullptr;
    QSystemTrayIcon* m_TrayIcon = nullptr;
    QMenu* m_TrayMenu = nullptr;
    QDialog* m_FrpProgressDialog = nullptr;
    QProgressBar* m_FrpProgressBar = nullptr;
    ElaText* m_FrpProgressLabel = nullptr;
    bool m_IsQuitting = false;
};

#endif // MAINWINDOW_H
