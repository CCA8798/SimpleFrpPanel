#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

class QResizeEvent;
class HomePage;
class ServerTunnelPage;
class ServerUserPage;
class ClientTunnelPage;

// 主窗口：使用 Ela 的 ElaWindow（无边框 + 导航栏 + 自绘标题栏）。
// 左侧导航结构：
//   首页
//   【服务端】隧道管理 / 用户管理
//   【客户端】隧道管理
// 导航栏宽度固定为窗口宽度的 30%。
class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    HomePage* m_HomePage = nullptr;
    ServerTunnelPage* m_ServerTunnelPage = nullptr;
    ServerUserPage* m_ServerUserPage = nullptr;
    ClientTunnelPage* m_ClientTunnelPage = nullptr;
};

#endif // MAINWINDOW_H
