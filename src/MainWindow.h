#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

class QCloseEvent;
class QMenu;
class QResizeEvent;
class QSystemTrayIcon;
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
// 后台运行：点关闭按钮（叉号）时隐藏到系统托盘，仅可通过托盘右键菜单"退出"关闭程序。
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

    HomePage* m_HomePage = nullptr;
    ServerTunnelPage* m_ServerTunnelPage = nullptr;
    ServerUserPage* m_ServerUserPage = nullptr;
    TrafficPage* m_TrafficPage = nullptr;
    ClientTunnelPage* m_ClientTunnelPage = nullptr;
    QSystemTrayIcon* m_TrayIcon = nullptr;
    QMenu* m_TrayMenu = nullptr;
    bool m_IsQuitting = false;
};

#endif // MAINWINDOW_H
