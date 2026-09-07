#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QProgressBar>
#include <QResizeEvent>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

#include "ElaCheckBox.h"
#include "ElaContentDialog.h"
#include "ElaIconButton.h"
#include "ElaMessageBar.h"
#include "ElaProgressBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "FrpUpdater.h"
#include "HomePage.h"
#include "Information.h"
#include "ServerTunnelPage.h"
#include "ServerUserPage.h"
#include "SettingsDialog.h"
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

    // 读取关闭行为等设置
    g_GlobalInformation.loadSettings();

    // 标题栏右上角：设置按钮（齿轮），紧贴日/夜主题切换按钮左侧
    // （先隐藏无用的"置顶"按钮，让右侧按钮组从主题切换按钮开始）
    setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
    ElaIconButton* settingsButton = new ElaIconButton(ElaIconType::Gear, 18, 40, 30, this);
    settingsButton->setFixedSize(40, 30);
    settingsButton->setToolTip(tr("设置"));
    connect(settingsButton, &ElaIconButton::clicked, this, [this]() {
        showSettingsDialog(this);
    });
    // 内置按钮在标题栏中均经"垂直布局(按钮+stretch)"排版：按钮贴顶、不随栏高拉伸。
    // 自定义区直接插入主布局会被拉伸导致与主题按钮不平齐，故同样包一层容器
    QWidget* settingsWrap = new QWidget(this);
    QVBoxLayout* settingsWrapLayout = new QVBoxLayout(settingsWrap);
    settingsWrapLayout->setContentsMargins(0, 0, 0, 0);
    settingsWrapLayout->setSpacing(0);
    settingsWrapLayout->addWidget(settingsButton);
    settingsWrapLayout->addStretch();
    setCustomWidget(ElaAppBarType::RightArea, settingsWrap);

    // 首页
    m_HomePage = new HomePage(this);
    addPageNode(tr("首页"), m_HomePage, ElaIconType::House);

    // 【服务端】
    QString serverKey;
    addExpanderNode(tr("服务端"), serverKey, ElaIconType::Server);
    m_ServerTunnelPage = new ServerTunnelPage(this);
    addPageNode(tr("隧道管理"), m_ServerTunnelPage, serverKey, ElaIconType::NetworkWired);
    m_ServerUserPage = new ServerUserPage(this);
    addPageNode(tr("用户管理"), m_ServerUserPage, serverKey, ElaIconType::Users);

    // 服务端 · 流量统计
    m_TrafficPage = new TrafficPage(this);
    addPageNode(tr("流量统计"), m_TrafficPage, serverKey, ElaIconType::ChartPie);

    // 【客户端】
    QString clientKey;
    addExpanderNode(tr("客户端"), clientKey, ElaIconType::Laptop);
    m_ClientTunnelPage = new ClientTunnelPage(this);
    addPageNode(tr("隧道管理"), m_ClientTunnelPage, clientKey, ElaIconType::NetworkWired);

    // 左侧导航栏宽度 = 窗口宽度的 30%
    setNavigationBarWidth(qRound(width() * 0.3));

    // 后台运行：右上角关闭按钮 / Alt+F4 等关闭动作统一走询问流程
    // （是否弹询问、记忆动作由 config.ini 设置控制）
    setupTrayIcon();
    setIsDefaultClosed(false);
    connect(this, &MainWindow::closeButtonClicked, this, &MainWindow::handleCloseRequest);

    // 内置 frp 更新检查：主窗口显示 1.5 秒后开始（联网，不阻塞启动）
    m_FrpUpdater = new FrpUpdater(this);
    connect(m_FrpUpdater, &FrpUpdater::checkFinished, this,
            &MainWindow::onFrpCheckFinished);
    connect(m_FrpUpdater, &FrpUpdater::downloadProgress, this,
            &MainWindow::setFrpProgressValue);
    connect(m_FrpUpdater, &FrpUpdater::updateProgress, this, &MainWindow::setFrpProgressText);
    connect(m_FrpUpdater, &FrpUpdater::updateFinished, this, [this](bool ok, const QString& message) {
        closeFrpProgress();
        if (ok)
        {
            ElaMessageBar::success(ElaMessageBarType::TopRight, tr("frp 更新"), message, 3500, this);
        }
        else
        {
            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("frp 更新失败"), message, 5000, this);
        }
    });
    QTimer::singleShot(1500, this, [this]() {
        if (m_FrpUpdater)
        {
            m_FrpUpdater->checkForUpdates();
        }
    });
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
    m_TrayIcon->setToolTip(tr("SimpleFrpPanel - FRP 管理面板"));

    m_TrayMenu = new QMenu(this);
    QAction* showAction = m_TrayMenu->addAction(tr("显示主界面"));
    m_TrayMenu->addSeparator();
    QAction* quitAction = m_TrayMenu->addAction(tr("退出"));

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
        tr("SimpleFrpPanel"),
        tr("程序已最小化到系统托盘，右键托盘图标可选择退出"),
        QSystemTrayIcon::Information, 2000);
}

void MainWindow::handleCloseRequest()
{
    if (m_IsQuitting)
    {
        qApp->quit();
        return;
    }
    // 无托盘环境：关闭即退出
    if (!m_TrayIcon || !m_TrayIcon->isVisible())
    {
        m_IsQuitting = true;
        close();
        return;
    }
    // 询问已关闭：按记忆动作直接执行
    if (!g_GlobalInformation.confirmOnClose)
    {
        if (g_GlobalInformation.closeAction == QStringLiteral("quit"))
        {
            m_IsQuitting = true;
            qApp->quit();
        }
        else
        {
            hideToTray();
        }
        return;
    }
    showClosePrompt();
}

void MainWindow::showClosePrompt()
{
    ElaContentDialog* dialog = new ElaContentDialog(this);
    dialog->setLeftButtonText(tr("取消"));
    dialog->setMiddleButtonText(tr("直接退出"));
    dialog->setRightButtonText(tr("后台运行"));
    dialog->resize(460, 210);

    QWidget* centralWidget = new QWidget(dialog);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(18, 10, 18, 12);
    centralLayout->setSpacing(6);

    ElaText* titleText = new ElaText(tr("关闭窗口后要如何运行？"), centralWidget);
    // 注意：setTextStyle(Title) 会把字号强制改为 28px，这里手动控制字号与字重
    titleText->setTextPixelSize(18);
    QFont titleFont = titleText->font();
    titleFont.setWeight(QFont::DemiBold);
    titleText->setFont(titleFont);
    centralLayout->addWidget(titleText);
    centralLayout->addSpacing(6);

    // 说明文字拆为两个单行 ElaText（垂直 Fixed），避免翻译后换行文本被裁剪
    ElaText* hintLine1 = new ElaText(tr("后台运行：最小化到系统托盘继续服务"), centralWidget);
    ElaText* hintLine2 = new ElaText(tr("直接退出：结束程序"), centralWidget);
    const QList<ElaText*> hintLabels = {hintLine1, hintLine2};
    for (ElaText* hint : hintLabels)
    {
        hint->setTextPixelSize(12);
        hint->setWordWrap(false);
        hint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        hint->setFixedHeight(20);
        centralLayout->addWidget(hint);
    }
    centralLayout->addSpacing(8);

    ElaCheckBox* keepAskingBox = new ElaCheckBox(tr("下次关闭继续弹出此提示"), centralWidget);
    keepAskingBox->setChecked(true);
    QFont checkFont = keepAskingBox->font();
    checkFont.setPixelSize(13);
    keepAskingBox->setFont(checkFont);
    centralLayout->addWidget(keepAskingBox);
    centralLayout->addStretch();

    dialog->setCentralWidget(centralWidget);

    // 三个按钮行为："取消"= 什么都不做；两个动作按钮在需要时记忆选择
    const auto finishWith = [this, dialog, keepAskingBox](const QString& action) {
        if (!keepAskingBox->isChecked())
        {
            g_GlobalInformation.confirmOnClose = false;
            g_GlobalInformation.closeAction = action;
            g_GlobalInformation.saveSettings();
        }
        if (action == QStringLiteral("quit"))
        {
            dialog->close();
            m_IsQuitting = true;
            qApp->quit();
        }
        else
        {
            hideToTray();
        }
    };
    connect(dialog, &ElaContentDialog::leftButtonClicked, dialog, &ElaContentDialog::close);
    connect(dialog, &ElaContentDialog::middleButtonClicked, this,
            [finishWith]() { finishWith(QStringLiteral("quit")); });
    connect(dialog, &ElaContentDialog::rightButtonClicked, this,
            [finishWith]() { finishWith(QStringLiteral("tray")); });

    // 空文本按钮隐藏（此处无，仅为与项目其他对话框保持一致写法）
    const QList<ElaPushButton*> buttons = dialog->findChildren<ElaPushButton*>();
    for (ElaPushButton* button : buttons)
    {
        if (button->text().isEmpty())
        {
            button->setVisible(false);
        }
    }

    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 真正退出（托盘菜单"退出"/询问中选择直接退出/无托盘环境）
    if (m_IsQuitting || !m_TrayIcon || !m_TrayIcon->isVisible())
    {
        ElaWindow::closeEvent(event);
        return;
    }
    // 普通关闭：拦截并走询问/记忆动作流程
    event->ignore();
    handleCloseRequest();
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

void MainWindow::onFrpCheckFinished(const QString& currentVersion,
                                    const QString& latestVersion, const QString& downloadUrl,
                                    const QString& message)
{
    Q_UNUSED(message)

    // 场景 1：未内置 frp（frp/ 目录或 exe 缺失）——统一弹对话框询问是否尝试下载。
    // 即便刚才联网未成功（拿不到下载地址），点击后也会重新尝试联网获取。
    if (currentVersion.isEmpty())
    {
        const QString bodyText = (!latestVersion.isEmpty())
                                     ? tr("程序目录 frp\\ 中未找到 frpc.exe / frps.exe。\n"
                                          "是否立即下载最新版 frp %1？")
                                           .arg(latestVersion)
                                     : tr("程序目录 frp\\ 中未找到 frpc.exe / frps.exe。\n"
                                          "是否尝试联网下载最新版 frp？");
        showFrpConfirmDialog(tr("未找到内置 frp"), bodyText, tr("尝试下载"), downloadUrl,
                             tr("frp 下载"));
        return;
    }

    // 场景 2：已内置但发现新版本
    if (!latestVersion.isEmpty() && !downloadUrl.isEmpty() && m_FrpUpdater)
    {
        showFrpConfirmDialog(
            tr("发现新版本 frp %1").arg(latestVersion),
            tr("当前内置版本：v%1\n更新将替换程序目录 frp\\ 下的 frpc.exe 与 frps.exe。")
                .arg(currentVersion),
            tr("立即更新"), downloadUrl, tr("frp 更新"));
    }
    // 其余（已最新）：静默
}

void MainWindow::showFrpConfirmDialog(const QString& titleText, const QString& bodyText,
                                      const QString& actionButtonText, const QString& downloadUrl,
                                      const QString& actionBarTitle)
{
    ElaContentDialog* dialog = new ElaContentDialog(this);
    dialog->setLeftButtonText(tr("稍后"));
    dialog->setMiddleButtonText(QString());
    dialog->setRightButtonText(actionButtonText);
    dialog->resize(480, 210);

    QWidget* centralWidget = new QWidget(dialog);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(18, 14, 18, 12);
    centralLayout->setSpacing(6);

    ElaText* title = new ElaText(titleText, centralWidget);
    title->setTextPixelSize(16);
    QFont titleFont = title->font();
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    centralLayout->addWidget(title);
    centralLayout->addSpacing(6);

    ElaText* body = new ElaText(bodyText, centralWidget);
    body->setTextPixelSize(12);
    body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    body->setFixedHeight(40);
    centralLayout->addWidget(body);
    centralLayout->addStretch();
    dialog->setCentralWidget(centralWidget);

    // 空文本按钮隐藏
    const QList<ElaPushButton*> buttons = dialog->findChildren<ElaPushButton*>();
    for (ElaPushButton* button : buttons)
    {
        if (button->text().isEmpty())
        {
            button->setVisible(false);
        }
    }
    connect(dialog, &ElaContentDialog::leftButtonClicked, dialog, &ElaContentDialog::close);
    connect(dialog, &ElaContentDialog::rightButtonClicked, this,
            [this, downloadUrl, actionBarTitle]() {
                if (!m_FrpUpdater)
                {
                    return;
                }
                showFrpProgress(tr("正在连接，获取 frp 下载信息…"));
                if (downloadUrl.isEmpty())
                {
                    // 未能拿到下载地址：重新联网获取后再下载
                    m_FrpUpdater->downloadLatest();
                }
                else
                {
                    m_FrpUpdater->performUpdate(downloadUrl);
                }
                Q_UNUSED(actionBarTitle)
            });
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::showFrpProgress(const QString& initialText)
{
    if (!m_FrpProgressDialog)
    {
        m_FrpProgressDialog = new QDialog(this);
        m_FrpProgressDialog->setWindowTitle(tr("frp 下载"));
        m_FrpProgressDialog->setModal(false);
        m_FrpProgressDialog->setFixedWidth(420);

        QWidget* content = new QWidget(m_FrpProgressDialog);
        QVBoxLayout* layout = new QVBoxLayout(content);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        m_FrpProgressLabel = new ElaText(initialText, content);
        m_FrpProgressLabel->setTextPixelSize(12);

        m_FrpProgressBar = new ElaProgressBar(content);
        m_FrpProgressBar->setRange(0, 100);
        m_FrpProgressBar->setValue(0);
        m_FrpProgressBar->setFixedHeight(8);
        m_FrpProgressBar->setTextVisible(false);

        layout->addWidget(m_FrpProgressLabel);
        layout->addWidget(m_FrpProgressBar);

        QVBoxLayout* outer = new QVBoxLayout(m_FrpProgressDialog);
        outer->setContentsMargins(12, 10, 12, 12);
        outer->addWidget(content);
    }
    else
    {
        m_FrpProgressLabel->setText(initialText);
    }
    m_FrpProgressBar->setRange(0, 100);
    m_FrpProgressBar->setValue(0);
    m_FrpProgressDialog->show();
    m_FrpProgressDialog->raise();
    // 置于父窗口中央
    const QPoint globalCenter = mapToGlobal(rect().center());
    m_FrpProgressDialog->move(globalCenter.x() - m_FrpProgressDialog->width() / 2,
                              globalCenter.y() - m_FrpProgressDialog->height() / 2);
}

void MainWindow::setFrpProgressValue(qint64 received, qint64 total)
{
    if (!m_FrpProgressBar || !m_FrpProgressDialog || !m_FrpProgressDialog->isVisible())
    {
        return;
    }
    if (total <= 0)
    {
        m_FrpProgressBar->setRange(0, 0); // 不确定进度（忙碌）
    }
    else
    {
        m_FrpProgressBar->setRange(0, 100);
        const int percent = static_cast<int>((received * 100) / total);
        m_FrpProgressBar->setValue(qBound(0, percent, 100));
    }
}

void MainWindow::setFrpProgressText(const QString& text)
{
    if (m_FrpProgressLabel && m_FrpProgressDialog && m_FrpProgressDialog->isVisible())
    {
        m_FrpProgressLabel->setText(text);
    }
}

void MainWindow::closeFrpProgress()
{
    if (m_FrpProgressDialog)
    {
        m_FrpProgressDialog->close();
        m_FrpProgressDialog->deleteLater();
        m_FrpProgressDialog = nullptr;
        m_FrpProgressBar = nullptr;
        m_FrpProgressLabel = nullptr;
    }
}
