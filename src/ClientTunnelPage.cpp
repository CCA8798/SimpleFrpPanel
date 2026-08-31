#include "ClientTunnelPage.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QIntValidator>
#include <QJsonDocument>
#include <QSettings>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "ElaContentDialog.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "FrpsManager.h"
#include "PanelClient.h"
#include "StatusDotDelegate.h"
#include "TunnelEditDialog.h"
#include "ui_ClientTunnelPage.h"

namespace {
const QString kSettingsSection = QStringLiteral("client");

QString jsonString(const QJsonObject& object, const QString& key)
{
    return object.value(key).toString();
}

int jsonInt(const QJsonObject& object, const QString& key, int defaultValue = 0)
{
    return object.value(key).toInt(defaultValue);
}
} // namespace

ClientTunnelPage::ClientTunnelPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::ClientTunnelPage())
    , m_Client(new PanelClient(this))
    , m_FrpcManager(new FrpsManager(QStringLiteral("client/frpcPath"), this))
    , m_TunnelModel(new QStandardItemModel(this))
{
    m_Ui->setupUi(this);

    // 顶部固定紧凑，日志区固定高度
    m_Ui->topFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_Ui->logFrame->setFixedHeight(130);

    const QList<QWidget*> topWidgets = {
        m_Ui->serverIpEdit, m_Ui->serverPortEdit, m_Ui->usernameEdit,
        m_Ui->passwordEdit, m_Ui->loginButton, m_Ui->logoutButton,
        m_Ui->frpcPathEdit, m_Ui->browseFrpcButton, m_Ui->frpcStartButton,
    };
    for (QWidget* widget : topWidgets)
    {
        widget->setFixedHeight(32);
    }
    m_Ui->connStatusLabel->setTextPixelSize(12);
    m_Ui->quotaLabel->setTextPixelSize(12);
    m_Ui->logTitleLabel->setTextPixelSize(12);
    m_Ui->frpcStatusLabel->setTextPixelSize(12);
    m_Ui->frpcPathEdit->setText(m_FrpcManager->frpsPath());

    // 轮询刷新：3 秒一次，服务端远程变更（其他客户端/服务端页）自动同步
    m_PollTimer = new QTimer(this);
    m_PollTimer->setInterval(3000);
    connect(m_PollTimer, &QTimer::timeout, this, [this]() {
        if (m_Client->isLoggedIn())
        {
            m_Client->requestTunnels();
        }
    });
    m_PollTimer->start();

    m_Ui->serverPortEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->logTextEdit->setMaximumBlockCount(2000);

    // 隧道表模型：开关 / 名称 / 协议 / 远端端口 / 目标 / 运行状况 / 备注
    m_TunnelModel->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("开关") << QStringLiteral("名称")
                      << QStringLiteral("协议") << QStringLiteral("远端端口")
                      << QStringLiteral("目标") << QStringLiteral("运行状况")
                      << QStringLiteral("备注"));
    m_Ui->tunnelTableView->setModel(m_TunnelModel);
    m_Ui->tunnelTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_Ui->tunnelTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_Ui->tunnelTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_Ui->tunnelTableView->verticalHeader()->setVisible(false);
    m_Ui->tunnelTableView->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_Ui->tunnelTableView->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_Ui->tunnelTableView->setItemDelegateForColumn(5, new StatusDotDelegate(m_Ui->tunnelTableView));

    connect(m_Ui->loginButton, &QPushButton::clicked, this, &ClientTunnelPage::onLoginClicked);
    connect(m_Ui->logoutButton, &QPushButton::clicked, this, &ClientTunnelPage::onLogoutClicked);
    connect(m_Ui->refreshButton, &QPushButton::clicked, this, &ClientTunnelPage::onRefreshClicked);
    connect(m_Ui->addTunnelButton, &QPushButton::clicked, this, &ClientTunnelPage::onAddTunnel);
    connect(m_Ui->editTunnelButton, &QPushButton::clicked, this, &ClientTunnelPage::onEditTunnel);
    connect(m_Ui->deleteTunnelButton, &QPushButton::clicked, this, &ClientTunnelPage::onDeleteTunnel);
    connect(m_Ui->clearLogButton, &QPushButton::clicked, this, &ClientTunnelPage::onClearLog);
    connect(m_Ui->browseFrpcButton, &QPushButton::clicked, this, &ClientTunnelPage::onBrowseFrpc);
    connect(m_Ui->frpcStartButton, &QPushButton::clicked, this, &ClientTunnelPage::onToggleFrpc);

    // frpc 进程状态与日志
    connect(m_FrpcManager, &FrpsManager::runningChanged, this, [this](bool) {
        updateFrpcStatusUi();
    });
    connect(m_FrpcManager, &FrpsManager::logMessage, this, &ClientTunnelPage::appendLog);

    // 客户端状态
    connect(m_Client, &PanelClient::connectionStateChanged, this, [this](bool connected) {
        m_Ui->connStatusLabel->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
        if (connected && m_PendingLogin)
        {
            m_PendingLogin = false;
            m_Client->login(m_Ui->usernameEdit->text().trimmed(),
                            m_Ui->passwordEdit->text());
        }
        updateLoginUi();
    });
    connect(m_Client, &PanelClient::loginSucceeded, this, [this](const QJsonObject& quota,
                                                                 const QJsonObject& serverInfo) {
        m_Quota = quota;
        m_ServerInfo = serverInfo;
        m_Ui->passwordEdit->clear();
        updateQuotaLabel();
        m_LastTunnelsSignature.clear();
        m_Client->requestTunnels();
        ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("登录成功"), 2000, this);
        updateLoginUi();
    });
    connect(m_Client, &PanelClient::loginFailed, this, [this](const QString& message) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("登录失败"),
                             message, 3000, this);
        updateLoginUi();
    });
    connect(m_Client, &PanelClient::loggedOut, this, [this]() {
        m_Tunnels = QJsonArray();
        m_Quota = QJsonObject();
        m_ServerInfo = QJsonObject();
        m_FrpsRunning = false;
        m_LastTunnelsSignature.clear();
        if (m_FrpcManager->isRunning())
        {
            m_FrpcManager->stop();
            appendLog(QStringLiteral("[%1] frpc 已停止（退出登录）")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        }
        refreshTunnelTable();
        updateQuotaLabel();
        updateLoginUi();
    });
    connect(m_Client, &PanelClient::tunnelsReceived, this, [this](const QJsonArray& tunnels,
                                                                  const QJsonObject& quota,
                                                                  bool frpsRunning) {
        m_Tunnels = tunnels;
        m_Quota = quota;
        m_FrpsRunning = frpsRunning;
        // 变化检测：数据没变就不重建表格，避免打断用户正在进行的操作
        const QString signature = QString::fromLatin1(QJsonDocument(m_Tunnels).toJson(QJsonDocument::Compact))
                                  + (m_FrpsRunning ? QStringLiteral("|1") : QStringLiteral("|0"));
        if (signature != m_LastTunnelsSignature)
        {
            m_LastTunnelsSignature = signature;
            refreshTunnelTable();
            updateQuotaLabel();
        }
        rebuildFrpcConfigIfRunning();
    });
    connect(m_Client, &PanelClient::commandSucceeded, this, [this](const QString& cmd) {
        if (cmd == QStringLiteral("add_tunnel"))
        {
            ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("隧道已添加"), 2000, this);
        }
        else if (cmd == QStringLiteral("update_tunnel"))
        {
            ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("隧道已更新"), 2000, this);
        }
        else if (cmd == QStringLiteral("delete_tunnel"))
        {
            ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("隧道已删除"), 2000, this);
        }
        m_Client->requestTunnels();
    });
    connect(m_Client, &PanelClient::commandFailed, this, [this](const QString&, const QString& message) {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             message, 3000, this);
        m_Client->requestTunnels(); // 刷新以还原状态
    });
    connect(m_Client, &PanelClient::logMessage, this, &ClientTunnelPage::appendLog);

    // 恢复上次登录信息
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                       QSettings::IniFormat);
    settings.beginGroup(kSettingsSection);
    m_Ui->serverIpEdit->setText(settings.value(QStringLiteral("host"), QStringLiteral("127.0.0.1")).toString());
    m_Ui->serverPortEdit->setText(settings.value(QStringLiteral("port"), QStringLiteral("7000")).toString());
    m_Ui->usernameEdit->setText(settings.value(QStringLiteral("username")).toString());
    settings.endGroup();

    updateLoginUi();
}

ClientTunnelPage::~ClientTunnelPage()
{
    delete m_Ui;
}

void ClientTunnelPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // 每次切换到本面板时刷新一次（登录状态下拉取最新隧道）
    if (m_Client->isLoggedIn())
    {
        m_LastTunnelsSignature.clear();
        m_Client->requestTunnels();
    }
}

void ClientTunnelPage::onLoginClicked()
{
    const QString host = m_Ui->serverIpEdit->text().trimmed();
    const QString portText = m_Ui->serverPortEdit->text().trimmed();
    const QString username = m_Ui->usernameEdit->text().trimmed();
    const QString password = m_Ui->passwordEdit->text();
    if (host.isEmpty())
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("请输入服务器 IP"), 2000, this);
        return;
    }
    bool portOk = false;
    const int portValue = portText.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("端口必须是 1-65535 的整数"), 2000, this);
        return;
    }
    if (username.isEmpty() || password.isEmpty())
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("请输入用户名和密码"), 2000, this);
        return;
    }

    // 记住服务器与用户名
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                       QSettings::IniFormat);
    settings.beginGroup(kSettingsSection);
    settings.setValue(QStringLiteral("host"), host);
    settings.setValue(QStringLiteral("port"), portText);
    settings.setValue(QStringLiteral("username"), username);
    settings.endGroup();

    appendLog(QStringLiteral("[%1] 正在连接 %2:%3")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), host, portText));
    m_PendingLogin = true;
    m_Client->connectToServer(host, static_cast<quint16>(portValue));
}

void ClientTunnelPage::onLogoutClicked()
{
    m_PendingLogin = false;
    m_Client->logout();
    appendLog(QStringLiteral("[%1] 已退出登录")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
}

void ClientTunnelPage::onRefreshClicked()
{
    m_Client->requestTunnels();
}

void ClientTunnelPage::onAddTunnel()
{
    if (!m_Client->isLoggedIn())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先登录"), 2000, this);
        return;
    }
    TunnelEditDialog dialog(false, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    m_Client->addTunnel(dialog.name(), dialog.protocol(), dialog.remotePort(),
                        dialog.localIp(), dialog.localPort(), dialog.customDomain(),
                        dialog.isEnabled(), dialog.remark());
}

void ClientTunnelPage::onEditTunnel()
{
    if (!m_Client->isLoggedIn())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先登录"), 2000, this);
        return;
    }
    const int id = selectedTunnelId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先在列表中选择要修改的隧道"), 2000, this);
        return;
    }
    for (const QJsonValue& value : m_Tunnels)
    {
        const QJsonObject tunnel = value.toObject();
        if (tunnel.value(QStringLiteral("id")).toInt() == id)
        {
            TunnelEditDialog dialog(true, this);
            dialog.setName(jsonString(tunnel, QStringLiteral("name")));
            dialog.setProtocol(jsonString(tunnel, QStringLiteral("protocol")));
            dialog.setRemotePort(jsonInt(tunnel, QStringLiteral("remotePort")));
            dialog.setLocalIp(jsonString(tunnel, QStringLiteral("localIp")));
            dialog.setLocalPort(jsonInt(tunnel, QStringLiteral("localPort")));
            dialog.setCustomDomain(jsonString(tunnel, QStringLiteral("customDomain")));
            dialog.setIsEnabled(tunnel.value(QStringLiteral("enabled")).toBool());
            dialog.setRemark(jsonString(tunnel, QStringLiteral("remark")));
            if (dialog.exec() != QDialog::Accepted)
            {
                return;
            }
            m_Client->updateTunnel(id, dialog.name(), dialog.protocol(), dialog.remotePort(),
                                   dialog.localIp(), dialog.localPort(), dialog.customDomain(),
                                   dialog.isEnabled(), dialog.remark());
            return;
        }
    }
    ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("未找到该隧道"), 2000, this);
}

void ClientTunnelPage::onDeleteTunnel()
{
    if (!m_Client->isLoggedIn())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先登录"), 2000, this);
        return;
    }
    const int id = selectedTunnelId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先在列表中选择要删除的隧道"), 2000, this);
        return;
    }
    ElaContentDialog* dialog = new ElaContentDialog(this);
    dialog->setLeftButtonText(QStringLiteral("取消"));
    dialog->setMiddleButtonText(QString());
    dialog->setRightButtonText(QStringLiteral("删除"));
    QWidget* centralWidget = new QWidget(dialog);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(15, 14, 15, 8);
    ElaText* titleText = new ElaText(QStringLiteral("确认删除"), centralWidget);
    titleText->setTextStyle(ElaTextType::Body);
    titleText->setTextPixelSize(15);
    ElaText* contentText = new ElaText(QStringLiteral("确定要删除该隧道吗？"), centralWidget);
    contentText->setTextStyle(ElaTextType::Body);
    contentText->setTextPixelSize(13);
    centralLayout->addWidget(titleText);
    centralLayout->addSpacing(2);
    centralLayout->addWidget(contentText);
    centralLayout->addStretch();
    dialog->setCentralWidget(centralWidget);
    const QList<ElaPushButton*> buttons = dialog->findChildren<ElaPushButton*>();
    for (ElaPushButton* button : buttons)
    {
        if (button->text().isEmpty())
        {
            button->setVisible(false);
        }
    }
    connect(dialog, &ElaContentDialog::leftButtonClicked, dialog, &ElaContentDialog::close);
    connect(dialog, &ElaContentDialog::rightButtonClicked, this, [this, id]() {
        m_Client->deleteTunnel(id);
    });
    dialog->exec();
    dialog->deleteLater();
}

void ClientTunnelPage::onClearLog()
{
    m_Ui->logTextEdit->clear();
}

void ClientTunnelPage::onBrowseFrpc()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 frpc.exe"), QString(),
        QStringLiteral("frpc (*.exe);;所有文件 (*)"));
    if (path.isEmpty())
    {
        return;
    }
    m_FrpcManager->setFrpsPath(path);
    m_Ui->frpcPathEdit->setText(path);
}

void ClientTunnelPage::onToggleFrpc()
{
    if (m_FrpcManager->isRunning())
    {
        m_FrpcManager->stop();
        appendLog(QStringLiteral("[%1] frpc 已停止")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        return;
    }
    if (!m_Client->isLoggedIn())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先登录"), 2000, this);
        return;
    }

    // 生成 frpc.toml：服务器地址用当前连接地址，端口/token 由登录响应下发
    const QString serverAddr = m_Ui->serverIpEdit->text().trimmed();
    const int serverPort = m_ServerInfo.value(QStringLiteral("frpsBindPort")).toInt(7000);
    const QString token = m_ServerInfo.value(QStringLiteral("frpsToken")).toString();
    if (serverAddr.isEmpty() || token.isEmpty())
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             QStringLiteral("缺少 frpc 连接参数，请重新登录"), 2500, this);
        return;
    }

    const QString configPath = QCoreApplication::applicationDirPath() + QStringLiteral("/frpc.toml");
    QString errorMessage;
    if (!FrpsManager::generateFrpcConfig(configPath, serverAddr,
                                         static_cast<quint16>(serverPort), token,
                                         m_Tunnels, &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 3000, this);
        return;
    }
    if (!m_FrpcManager->start(configPath, &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 3000, this);
        appendLog(QStringLiteral("[%1] frpc 启动失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    m_LastFrpcConfigSignature = frpcConfigSignature();
    appendLog(QStringLiteral("[%1] frpc 已启动 (配置: %2，服务器 %3:%4)")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       configPath, serverAddr)
                  .arg(serverPort));
}

void ClientTunnelPage::updateFrpcStatusUi()
{
    const bool running = m_FrpcManager->isRunning();
    m_Ui->frpcStartButton->setText(running ? QStringLiteral("停止") : QStringLiteral("启动"));
    m_Ui->frpcStatusLabel->setText(running ? QStringLiteral("运行中") : QStringLiteral("未运行"));
    m_Ui->frpcStatusLight->setColor(running ? QColor(0x4C, 0xAF, 0x50)
                                            : QColor(0x9E, 0x9E, 0x9E));
}

void ClientTunnelPage::rebuildFrpcConfigIfRunning()
{
    // 隧道增删改/开关后，若 frpc 在运行则重新生成配置并热重启
    if (!m_FrpcManager->isRunning() || !m_Client->isLoggedIn())
    {
        return;
    }
    // 配置签名未变化（轮询拉取的数据与上次一致）时绝不重启 frpc：
    // 重启会阻塞 UI 线程（进程终止/启动等待），每 3 秒轮询一次若都重启将导致整个软件卡死
    const QString signature = frpcConfigSignature();
    if (signature == m_LastFrpcConfigSignature)
    {
        return;
    }
    m_LastFrpcConfigSignature = signature;

    const QString serverAddr = m_Ui->serverIpEdit->text().trimmed();
    const int serverPort = m_ServerInfo.value(QStringLiteral("frpsBindPort")).toInt(7000);
    const QString token = m_ServerInfo.value(QStringLiteral("frpsToken")).toString();
    const QString configPath = QCoreApplication::applicationDirPath() + QStringLiteral("/frpc.toml");
    QString errorMessage;
    if (!FrpsManager::generateFrpcConfig(configPath, serverAddr,
                                         static_cast<quint16>(serverPort), token,
                                         m_Tunnels, &errorMessage))
    {
        appendLog(QStringLiteral("[%1] frpc 配置重新生成失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    m_FrpcManager->stop();
    if (!m_FrpcManager->start(configPath, &errorMessage))
    {
        appendLog(QStringLiteral("[%1] frpc 重启失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    appendLog(QStringLiteral("[%1] frpc 配置已更新并重启")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
}

QString ClientTunnelPage::frpcConfigSignature() const
{
    QString signature = m_Ui->serverIpEdit->text().trimmed()
                        + QLatin1Char('|')
                        + m_ServerInfo.value(QStringLiteral("frpsBindPort")).toString()
                        + QLatin1Char('|')
                        + m_ServerInfo.value(QStringLiteral("frpsToken")).toString()
                        + QLatin1Char('|');
    for (const QJsonValue& value : m_Tunnels)
    {
        const QJsonObject tunnel = value.toObject();
        if (!tunnel.value(QStringLiteral("enabled")).toBool())
        {
            continue;
        }
        signature += QStringLiteral("%1:%2:%3:%4:%5:%6;")
                         .arg(tunnel.value(QStringLiteral("name")).toString())
                         .arg(tunnel.value(QStringLiteral("protocol")).toString())
                         .arg(tunnel.value(QStringLiteral("remotePort")).toInt())
                         .arg(tunnel.value(QStringLiteral("localIp")).toString())
                         .arg(tunnel.value(QStringLiteral("localPort")).toInt())
                         .arg(tunnel.value(QStringLiteral("customDomain")).toString());
    }
    return signature;
}

void ClientTunnelPage::updateLoginUi()
{
    const bool loggedIn = m_Client->isLoggedIn();
    const bool connected = m_Client->isConnected();
    m_Ui->serverIpEdit->setEnabled(!loggedIn);
    m_Ui->serverPortEdit->setEnabled(!loggedIn);
    m_Ui->usernameEdit->setEnabled(!loggedIn);
    m_Ui->passwordEdit->setEnabled(!loggedIn);
    m_Ui->loginButton->setEnabled(!loggedIn);
    m_Ui->logoutButton->setEnabled(loggedIn);
    m_Ui->refreshButton->setEnabled(loggedIn);
    m_Ui->addTunnelButton->setEnabled(loggedIn);
    m_Ui->editTunnelButton->setEnabled(loggedIn);
    m_Ui->deleteTunnelButton->setEnabled(loggedIn);
    m_Ui->frpcStartButton->setEnabled(loggedIn);
    m_Ui->connStatusLabel->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
}

void ClientTunnelPage::updateQuotaLabel()
{
    if (!m_Client->isLoggedIn())
    {
        m_Ui->quotaLabel->setText(QStringLiteral("未登录"));
        return;
    }
    const int remoteMin = jsonInt(m_Quota, QStringLiteral("remoteMin"));
    const int remoteMax = jsonInt(m_Quota, QStringLiteral("remoteMax"));
    const int localMin = jsonInt(m_Quota, QStringLiteral("localMin"));
    const int localMax = jsonInt(m_Quota, QStringLiteral("localMax"));
    const int maxCount = jsonInt(m_Quota, QStringLiteral("maxCount"));
    const int usedCount = jsonInt(m_Quota, QStringLiteral("usedCount"));
    QString text = QStringLiteral("配额：远端 %1-%2 | 本地 %3-%4 | 已用 %5/%6")
                       .arg(remoteMin)
                       .arg(remoteMax)
                       .arg(localMin)
                       .arg(localMax)
                       .arg(usedCount)
                       .arg(maxCount);
    const QString publicIp = jsonString(m_ServerInfo, QStringLiteral("publicIp"));
    const QString publicPort = jsonString(m_ServerInfo, QStringLiteral("publicPort"));
    if (!publicIp.isEmpty() || !publicPort.isEmpty())
    {
        text += QStringLiteral(" | 服务端公网 %1:%2").arg(publicIp, publicPort);
    }
    m_Ui->quotaLabel->setText(text);
}

void ClientTunnelPage::refreshTunnelTable()
{
    // 清理旧的开关控件
    for (int row = 0; row < m_TunnelModel->rowCount(); ++row)
    {
        m_Ui->tunnelTableView->setIndexWidget(m_TunnelModel->index(row, 0), nullptr);
    }
    m_TunnelModel->setRowCount(0);

    for (const QJsonValue& value : m_Tunnels)
    {
        const QJsonObject tunnel = value.toObject();
        const int tunnelId = jsonInt(tunnel, QStringLiteral("id"));
        const QString name = jsonString(tunnel, QStringLiteral("name"));
        const QString protocol = jsonString(tunnel, QStringLiteral("protocol"));
        const int remotePort = jsonInt(tunnel, QStringLiteral("remotePort"));
        const QString localIp = jsonString(tunnel, QStringLiteral("localIp"));
        const int localPort = jsonInt(tunnel, QStringLiteral("localPort"));
        const QString customDomain = jsonString(tunnel, QStringLiteral("customDomain"));
        const bool enabled = tunnel.value(QStringLiteral("enabled")).toBool();
        const QString status = jsonString(tunnel, QStringLiteral("status"));
        const QString remark = jsonString(tunnel, QStringLiteral("remark"));

        const int row = m_TunnelModel->rowCount();
        m_TunnelModel->insertRow(row);

        QStandardItem* switchItem = new QStandardItem();
        switchItem->setData(tunnelId, Qt::UserRole);
        switchItem->setTextAlignment(Qt::AlignCenter);
        switchItem->setSizeHint(QSize(56, 26));
        m_TunnelModel->setItem(row, 0, switchItem);

        QStandardItem* nameItem = new QStandardItem(name);
        nameItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 1, nameItem);

        QStandardItem* protocolItem = new QStandardItem(protocol);
        protocolItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 2, protocolItem);

        QStandardItem* portItem = new QStandardItem(
            (protocol == QStringLiteral("http") || protocol == QStringLiteral("https"))
                ? QStringLiteral("-")
                : QString::number(remotePort));
        portItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 3, portItem);

        QString targetText = QStringLiteral("%1:%2").arg(localIp).arg(localPort);
        if (!customDomain.trimmed().isEmpty())
        {
            targetText += QStringLiteral(" / ") + customDomain.trimmed();
        }
        QStandardItem* targetItem = new QStandardItem(targetText);
        targetItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 4, targetItem);

        QStandardItem* statusItem = new QStandardItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 5, statusItem);

        QStandardItem* remarkItem = new QStandardItem(remark);
        remarkItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 6, remarkItem);

        // 行内开关：向服务端发送启停指令（具体端口由客户端自选，服务端校验配额）
        ElaToggleSwitch* toggleSwitch = new ElaToggleSwitch(m_Ui->tunnelTableView);
        toggleSwitch->setIsToggled(!enabled);
        toggleSwitch->setIsToggled(enabled);
        m_Ui->tunnelTableView->setIndexWidget(m_TunnelModel->index(row, 0), toggleSwitch);
        connect(toggleSwitch, &ElaToggleSwitch::toggled, this, [this, tunnelId](bool checked) {
            m_Client->setTunnelEnabled(tunnelId, checked);
            // 乐观更新状态文本，服务端确认后 tunnelsReceived 会整体刷新
            const QString newStatus = checked
                                          ? (m_FrpsRunning ? QStringLiteral("运行中")
                                                           : QStringLiteral("未运行"))
                                          : QStringLiteral("已禁用");
            for (int statusRow = 0; statusRow < m_TunnelModel->rowCount(); ++statusRow)
            {
                if (m_TunnelModel->item(statusRow, 0)->data(Qt::UserRole).toInt() == tunnelId)
                {
                    if (QStandardItem* statusItem = m_TunnelModel->item(statusRow, 5))
                    {
                        statusItem->setText(newStatus);
                    }
                    break;
                }
            }
        });
    }
}

void ClientTunnelPage::appendLog(const QString& text)
{
    m_Ui->logTextEdit->appendPlainText(text);
}

int ClientTunnelPage::selectedTunnelId() const
{
    const QModelIndexList selectedRows = m_Ui->tunnelTableView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
    {
        return -1;
    }
    const QModelIndex index = selectedRows.first();
    const QStandardItem* item = m_TunnelModel->item(index.row(), 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}
