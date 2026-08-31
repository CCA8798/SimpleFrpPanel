#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QSystemTrayIcon>

#include "HomePage.h"
#include "ServerTunnelPage.h"
#include "ServerUserPage.h"
#include "TrafficPage.h"
#include "ClientTunnelPage.h"

namespace {
// 程序化绘制应用图标（Ela 主色圆角方块 + "F"），避免依赖资源文件
QIcon createAppIcon()
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0x0E, 0x8A, 0xE8));
    painter.drawRoundedRect(2, 2, 60, 60, 14, 14);
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(38);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("F"));
    return QIcon(pixmap);
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    setWindowTitle(QStringLiteral("SimpleFrpPanel"));
    resize(1000, 700);
    moveToCenter();
    setUserInfoCardVisible(false);
    setWindowIcon(createAppIcon());

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

    // 服务端 · 流量统计
    m_TrafficPage = new TrafficPage(this);
    addPageNode(QStringLiteral("流量统计"), m_TrafficPage, serverKey, ElaIconType::ChartPie);

    // 【客户端】
    QString clientKey;
    addExpanderNode(QStringLiteral("客户端"), clientKey, ElaIconType::Laptop);
    m_ClientTunnelPage = new ClientTunnelPage(this);
    addPageNode(QStringLiteral("隧道管理"), m_ClientTunnelPage, clientKey, ElaIconType::NetworkWired);

    // 左侧导航栏宽度 = 窗口宽度的 30%
    setNavigationBarWidth(qRound(width() * 0.3));

    // 后台运行：关闭按钮（叉号）改为隐藏到系统托盘，仅托盘右键"退出"可关闭程序
    setupTrayIcon();
    setIsDefaultClosed(false);
    connect(this, &MainWindow::closeButtonClicked, this, &MainWindow::hideToTray);
}

MainWindow::~MainWindow()
{
    // 退出时清理托盘图标
    if (m_TrayIcon)
    {
        m_TrayIcon->hide();
    }
}

void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        return; // 无系统托盘环境：保持正常关闭行为
    }
    m_TrayIcon = new QSystemTrayIcon(createAppIcon(), this);
    m_TrayIcon->setToolTip(QStringLiteral("SimpleFrpPanel - FRP 管理面板"));

    m_TrayMenu = new QMenu(this);
    QAction* showAction = m_TrayMenu->addAction(QStringLiteral("显示主界面"));
    m_TrayMenu->addSeparator();
    QAction* quitAction = m_TrayMenu->addAction(QStringLiteral("退出"));

    connect(showAction, &QAction::triggered, this, [this]() {
        show();
        raise();
        activateWindow();
    });
    connect(quitAction, &QAction::triggered, this, [this]() {
        m_IsQuitting = true;
        qApp->quit();
    });
    m_TrayIcon->setContextMenu(m_TrayMenu);

    // 左键单击/双击托盘图标：恢复主界面
    connect(m_TrayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                {
                    show();
                    raise();
                    activateWindow();
                }
            });
    m_TrayIcon->show();
}

void MainWindow::hideToTray()
{
    // 无托盘可用（如系统托盘被禁用）时回退为真正关闭
    if (!m_TrayIcon || !m_TrayIcon->isVisible())
    {
        m_IsQuitting = true;
        close();
        return;
    }
    hide();
    m_TrayIcon->showMessage(
        QStringLiteral("SimpleFrpPanel"),
        QStringLiteral("程序已最小化到系统托盘，右键托盘图标可选择退出"),
        QSystemTrayIcon::Information, 2000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Alt+F4 / 任务栏关闭 / 程序退出：非显式退出时一律隐藏到托盘
    if (m_IsQuitting || !m_TrayIcon || !m_TrayIcon->isVisible())
    {
        ElaWindow::closeEvent(event);
        return;
    }
    event->ignore();
    hideToTray();
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
