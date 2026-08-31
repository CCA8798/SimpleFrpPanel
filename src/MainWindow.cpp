#include "MainWindow.h"

#include <QResizeEvent>

#include "HomePage.h"
#include "ServerTunnelPage.h"
#include "ServerUserPage.h"
#include "ClientTunnelPage.h"

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    setWindowTitle(QStringLiteral("SimpleFrpPanel"));
    resize(1000, 700);
    moveToCenter();
    setUserInfoCardVisible(false);

    // 首页
    m_HomePage = new HomePage(this);
    addPageNode(QStringLiteral("首页"), m_HomePage, ElaIconType::House);

    // 【服务端】
    QString serverKey;
    addExpanderNode(QStringLiteral("服务端"), serverKey, ElaIconType::Server);
    m_ServerTunnelPage = new ServerTunnelPage(this);
    addPageNode(QStringLiteral("隧道管理"), m_ServerTunnelPage, serverKey, ElaIconType::NetworkWired);
    m_ServerUserPage = new ServerUserPage(this);
    addPageNode(QStringLiteral("用户管理"), m_ServerUserPage, serverKey, ElaIconType::Users);

    // 【客户端】
    QString clientKey;
    addExpanderNode(QStringLiteral("客户端"), clientKey, ElaIconType::Laptop);
    m_ClientTunnelPage = new ClientTunnelPage(this);
    addPageNode(QStringLiteral("隧道管理"), m_ClientTunnelPage, clientKey, ElaIconType::NetworkWired);

    // 左侧导航栏宽度 = 窗口宽度的 30%
    setNavigationBarWidth(qRound(width() * 0.3));
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    ElaWindow::resizeEvent(event);
    const int targetWidth = qRound(width() * 0.3);
    if (getNavigationBarWidth() != targetWidth)
    {
        setNavigationBarWidth(targetWidth);
    }
}
