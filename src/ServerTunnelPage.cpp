#include "ServerTunnelPage.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QFileDialog>
#include <QHeaderView>
#include <QIntValidator>
#include <QRandomGenerator>
#include <QSettings>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "DatabaseManager.h"
#include "ElaContentDialog.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToggleSwitch.h"
#include "FrpsManager.h"
#include "FrpUpdater.h"
#include "Information.h"
#include "PanelApiServer.h"
#include "PortChecker.h"
#include "StatusDotDelegate.h"
#include "TrafficMonitor.h"
#include "TunnelEditDialog.h"
#include "ui_ServerTunnelPage.h"

namespace {
const QString kSettingFrpsBindPort = QStringLiteral("frps_bind_port");
const QString kSettingFrpsToken = QStringLiteral("frps_token");
const QString kSettingPublicPort = QStringLiteral("public_port");
const QString kSettingFrpsWebPort = QStringLiteral("frps_web_port");
const QString kSettingFrpsWebUser = QStringLiteral("frps_web_user");
const QString kSettingFrpsWebPassword = QStringLiteral("frps_web_password");
// 本页"上次选择的数据库/用户"记忆（config.ini）
const QString kServerStateSection = QStringLiteral("server_state");

QString randomHexToken(int byteCount)
{
    QByteArray bytes;
    bytes.reserve(byteCount);
    while (bytes.size() < byteCount)
    {
        const quint32 value = QRandomGenerator::system()->generate();
        bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    return QString::fromLatin1(bytes.left(byteCount).toHex());
}

bool isExpired(const QString& expireAt)
{
    if (expireAt.trimmed().isEmpty())
    {
        return false;
    }
    const QDate date = QDate::fromString(expireAt.trimmed(), QStringLiteral("yyyy-MM-dd"));
    return date.isValid() && date < QDate::currentDate();
}
} // namespace

ServerTunnelPage::ServerTunnelPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::ServerTunnelPage())
    , m_DatabaseManager(new DatabaseManager(this))
    , m_FrpsManager(new FrpsManager(QStringLiteral("frps/path"), this))
    , m_TunnelModel(new QStandardItemModel(this))
{
    m_Ui->setupUi(this);

    // 读取上次选择的数据库/用户记忆：必须在首次加载下拉框之前，
    // 否则下拉初始化会先触发一次保存，把记忆覆盖成默认值
    {
        QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                           QSettings::IniFormat);
        settings.beginGroup(kServerStateSection);
        m_RememberedDbName = settings.value(QStringLiteral("dbName")).toString();
        m_RememberedUserId = settings.value(QStringLiteral("userId"), -1).toInt();
        settings.endGroup();
    }

    // 标签统一使用 Ela 主题文字（跟随黑夜/白天切换），字号 13px
    const QList<ElaText*> pageLabels = findChildren<ElaText*>();
    for (ElaText* label : pageLabels)
    {
        label->setTextPixelSize(13);
    }
    // 状态类小字随后在各自更新函数中覆盖为 12px

    // 面板 API 服务：为客户端隧道管理提供 TCP 接口
    m_PanelApiServer = new PanelApiServer(m_DatabaseManager, m_FrpsManager, this);
    connect(m_PanelApiServer, &PanelApiServer::runningChanged, this, [this](bool) {
        updatePanelServiceUi();
        updateGlobalOverview();
    });
    connect(m_PanelApiServer, &PanelApiServer::logMessage, this, &ServerTunnelPage::appendLog);

    // 流量监控：从 frps 仪表盘 API 采样，写入流量记录表
    m_TrafficMonitor = new TrafficMonitor(m_DatabaseManager, this);
    connect(m_TrafficMonitor, &TrafficMonitor::logMessage, this, &ServerTunnelPage::appendLog);

    // 布局：顶部固定紧凑，中部自动扩展，日志区固定高度
    m_Ui->topFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_Ui->logFrame->setFixedHeight(150);

    // 顶部控件统一收窄高度，减少上方留空
    const QList<QWidget*> topWidgets = {
        m_Ui->dbComboBox, m_Ui->userComboBox, m_Ui->refreshButton,
        m_Ui->frpsPathEdit, m_Ui->browseButton, m_Ui->frpsPortEdit,
        m_Ui->frpsTokenEdit, m_Ui->startButton,
        m_Ui->remoteMinEdit, m_Ui->remoteMaxEdit, m_Ui->localMinEdit,
        m_Ui->localMaxEdit, m_Ui->maxPortCountEdit, m_Ui->saveQuotaButton,
        m_Ui->panelPortEdit, m_Ui->panelServiceButton,
        m_Ui->webPortEdit,
    };
    for (QWidget* widget : topWidgets)
    {
        widget->setFixedHeight(32);
    }
    m_Ui->topFrameLayout->setSpacing(4);

    // 顶部小字号文本
    m_Ui->frpsStatusLabel->setTextPixelSize(12);
    m_Ui->logTitleLabel->setTextPixelSize(12);
    m_Ui->panelServiceStatusLabel->setTextPixelSize(12);

    // 端口输入校验
    m_Ui->frpsPortEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->webPortEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->remoteMinEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->remoteMaxEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->localMinEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->localMaxEdit->setValidator(new QIntValidator(1, 65535, this));
    m_Ui->maxPortCountEdit->setValidator(new QIntValidator(1, 65535, this));

    // 隧道表模型：开关 / 名称 / 协议 / 远端端口 / 目标 / 运行状况 / 备注
    m_TunnelModel->setHorizontalHeaderLabels(
        QStringList() << tr("开关") << tr("名称")
                      << tr("协议") << tr("远端端口")
                      << tr("目标") << tr("运行状况")
                      << tr("备注"));
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
    // 运行状况列：彩色状态灯委托
    m_Ui->tunnelTableView->setItemDelegateForColumn(5, new StatusDotDelegate(m_Ui->tunnelTableView));

    // 日志面板限制行数
    m_Ui->logTextEdit->setMaximumBlockCount(2000);

    // 修复 ElaPlainTextEdit 黑夜模式背景不变色：
    // 库内样式表 background-color:transparent 会压制调色板 Base，文本区背景不跟随主题。
    // 双保险：① 显式设置 widget 与 viewport 的调色板 Base；② 样式表直接写主题背景色
    const auto updateLogTheme = [this](ElaThemeType::ThemeMode themeMode) {
        if (m_Ui->logTextEdit->toPlainText().isEmpty()) {
            if (themeMode==ElaThemeType::Light) {
                m_Ui->logTextEdit->setStyleSheet(
            QStringLiteral("background-color: %1; color: %2;")
                .arg(ElaThemeColor(eTheme->getThemeMode(), BasicBase).name(),
                     ElaThemeColor(eTheme->getThemeMode(), BasicTextInvert).name()));
            } else if (themeMode == ElaThemeType::Dark) {
                m_Ui->logTextEdit->setStyleSheet(
            QStringLiteral("background-color: %1; color: %2;")
                .arg(ElaThemeColor(eTheme->getThemeMode(), BasicBase).name(),
                     ElaThemeColor(eTheme->getThemeMode(), BasicTextInvert).name()));
            }
        }else {
            if (themeMode==ElaThemeType::Light) {
                m_Ui->logTextEdit->setStyleSheet(
            QStringLiteral("background-color: %1; color: %2;")
                .arg(ElaThemeColor(eTheme->getThemeMode(), BasicBase).name(),
                     ElaThemeColor(eTheme->getThemeMode(), BasicText).name()));
            } else if (themeMode == ElaThemeType::Dark) {
                m_Ui->logTextEdit->setStyleSheet(
            QStringLiteral("background-color: %1; color: %2;")
                .arg(ElaThemeColor(eTheme->getThemeMode(), BasicBase).name(),
                     ElaThemeColor(eTheme->getThemeMode(), BasicText).name()));
            }
        }
    };
    updateLogTheme(eTheme->getThemeMode());
    connect(eTheme, &ElaTheme::themeModeChanged, this, updateLogTheme);
    connect(m_Ui->logTextEdit,&QPlainTextEdit::textChanged,[this, updateLogTheme](){
        updateLogTheme(eTheme->getThemeMode());
    });

    connect(m_Ui->dbComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ServerTunnelPage::onCurrentDbChanged);
    connect(m_Ui->refreshButton, &QPushButton::clicked, this, &ServerTunnelPage::onRefreshDbComboBox);
    connect(m_Ui->userComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ServerTunnelPage::onCurrentUserChanged);
    connect(m_Ui->searchButton, &QPushButton::clicked, this, &ServerTunnelPage::onSearchTunnels);
    connect(m_Ui->searchLineEdit, &QLineEdit::returnPressed, this, &ServerTunnelPage::onSearchTunnels);
    connect(m_Ui->addTunnelButton, &QPushButton::clicked, this, &ServerTunnelPage::onAddTunnel);
    connect(m_Ui->editTunnelButton, &QPushButton::clicked, this, &ServerTunnelPage::onEditTunnel);
    connect(m_Ui->deleteTunnelButton, &QPushButton::clicked, this, &ServerTunnelPage::onDeleteTunnel);
    connect(m_Ui->browseButton, &QPushButton::clicked, this, &ServerTunnelPage::onBrowseFrps);
    connect(m_Ui->startButton, &QPushButton::clicked, this, &ServerTunnelPage::onToggleFrps);
    connect(m_Ui->clearLogButton, &QPushButton::clicked, this, &ServerTunnelPage::onClearLog);
    connect(m_Ui->saveQuotaButton, &QPushButton::clicked, this, &ServerTunnelPage::onSaveQuota);
    connect(m_Ui->panelServiceButton, &QPushButton::clicked, this, &ServerTunnelPage::onTogglePanelService);

    // frps 配置参数"编辑完即保存"：绑定端口 / Token / 仪表盘端口 / 面板端口无需点启动
    // 就会写入当前数据库，下次打开（重启应用或切库回来）自动恢复
    const auto saveOnEditFinished = [this](QLineEdit* edit, const QString& settingKey) {
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, settingKey]() {
            if (!m_DatabaseManager->isOpen())
            {
                return;
            }
            m_DatabaseManager->setSetting(settingKey, edit->text().trimmed());
        });
    };
    saveOnEditFinished(m_Ui->frpsPortEdit, kSettingFrpsBindPort);
    saveOnEditFinished(m_Ui->frpsTokenEdit, kSettingFrpsToken);
    saveOnEditFinished(m_Ui->webPortEdit, kSettingFrpsWebPort);
    saveOnEditFinished(m_Ui->panelPortEdit, kSettingPublicPort);
    // 面板端口修改后同步运行中的面板服务（端口确实变化时自动重启服务，不影响在线客户端）
    connect(m_Ui->panelPortEdit, &QLineEdit::editingFinished, this, [this]() {
        syncPanelServiceWithDb();
    });

    connect(m_FrpsManager, &FrpsManager::runningChanged, this, [this](bool) {
        // 只更新状态灯与状态列文本，绝不重建表格：
        // 整体重建会删除开关控件（发生在开关自己的鼠标事件栈内），导致开关失灵
        updateFrpsStatusUi();
        updateTunnelStatusColumn();
        updateGlobalOverview();
    });
    connect(m_FrpsManager, &FrpsManager::logMessage, this, &ServerTunnelPage::appendLog);

    // 轮询刷新：5 秒一次，客户端通过 API 的远程变更无需手动刷新
    m_PollTimer = new QTimer(this);
    m_PollTimer->setInterval(5000);
    connect(m_PollTimer, &QTimer::timeout, this, &ServerTunnelPage::onPollRefresh);
    m_PollTimer->start();

    onRefreshDbComboBox();
    updateGlobalOverview();

    // 恢复上次选择的数据库与用户（记忆在构造早期已读入，见上）
    applyRememberedUiState();
}

ServerTunnelPage::~ServerTunnelPage()
{
    delete m_Ui;
}

void ServerTunnelPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // 页面每次显示时刷新：及时感知其他页面（如用户管理）对库/用户的增删
    m_LastSignature.clear();
    onRefreshDbComboBox();
    m_LastSignature = stateSignature();
    updateGlobalOverview();
}

void ServerTunnelPage::onPollRefresh()
{
    // 只有数据真正变化才重建界面，避免打断用户正在进行的操作
    const QString signature = stateSignature();
    if (signature != m_LastSignature)
    {
        m_LastSignature = signature;
        onRefreshDbComboBox();
    }
    // 总览数据（含流量）无论界面是否变化都定时刷新
    updateGlobalOverview();
}

QString ServerTunnelPage::stateSignature() const
{
    QStringList parts = m_DatabaseManager->databaseFileNames();
    parts << QStringLiteral("|") << m_Ui->dbComboBox->currentText();
    if (m_DatabaseManager->isOpen())
    {
        const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
        for (const DatabaseManager::UserInfo& user : users)
        {
            parts << QStringLiteral("%1:%2:%3:%4:%5:%6:%7:%8")
                         .arg(user.id)
                         .arg(user.username)
                         .arg(user.isEnabled ? 1 : 0)
                         .arg(user.remotePortMin)
                         .arg(user.remotePortMax)
                         .arg(user.localPortMin)
                         .arg(user.localPortMax)
                         .arg(user.maxPortCount);
            if (user.id == m_CurrentUserId)
            {
                const QList<DatabaseManager::TunnelInfo> tunnels = m_DatabaseManager->queryTunnels(user.id);
                for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
                {
                    parts << QStringLiteral("%1:%2:%3:%4:%5:%6:%7:%8")
                                 .arg(tunnel.id)
                                 .arg(tunnel.name)
                                 .arg(tunnel.protocol)
                                 .arg(tunnel.remotePort)
                                 .arg(tunnel.localIp)
                                 .arg(tunnel.localPort)
                                 .arg(tunnel.customDomain)
                                 .arg(tunnel.isEnabled ? 1 : 0);
                }
            }
        }
    }
    parts << QStringLiteral("|") << m_DatabaseManager->getSetting(QStringLiteral("public_port"))
          << (m_FrpsManager->isRunning() ? QStringLiteral("1") : QStringLiteral("0"));
    return parts.join(QLatin1Char(','));
}

void ServerTunnelPage::onRefreshDbComboBox()
{
    const QString previousName = m_Ui->dbComboBox->currentText();
    m_Ui->dbComboBox->blockSignals(true);
    m_Ui->dbComboBox->clear();
    m_Ui->dbComboBox->addItems(m_DatabaseManager->databaseFileNames());
    const int index = m_Ui->dbComboBox->findText(previousName);
    m_Ui->dbComboBox->setCurrentIndex(index >= 0 ? index : 0);
    m_Ui->dbComboBox->blockSignals(false);
    onCurrentDbChanged();
}

void ServerTunnelPage::onCurrentDbChanged()
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        m_DatabaseManager->closeDatabase();
        m_TrafficMonitor->clearBaselines();
        m_CurrentUserId = -1;
        m_Ui->userComboBox->clear();
        m_Ui->frpsPathEdit->clear();
        m_Ui->frpsPortEdit->clear();
        m_Ui->frpsTokenEdit->clear();
        m_Ui->frpsStatusLabel->setText(tr("未运行"));
        m_Ui->remoteMinEdit->clear();
        m_Ui->remoteMaxEdit->clear();
        m_Ui->localMinEdit->clear();
        m_Ui->localMaxEdit->clear();
        m_Ui->maxPortCountEdit->clear();
        refreshTunnelTable();
        updateControlsEnabled();
        return;
    }

    if (m_DatabaseManager->currentDatabaseName() != fileName
        && !m_DatabaseManager->openDatabase(fileName))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             tr("打开数据库 %1 失败").arg(fileName), 2000, this);
        m_Ui->dbComboBox->blockSignals(true);
        m_Ui->dbComboBox->removeItem(m_Ui->dbComboBox->currentIndex());
        m_Ui->dbComboBox->blockSignals(false);
        onCurrentDbChanged();
        return;
    }

    // frps 设置：绑定端口与 Token（首次打开自动生成随机 Token）
    QString bindPort = m_DatabaseManager->getSetting(kSettingFrpsBindPort);
    if (bindPort.trimmed().isEmpty())
    {
        bindPort = QStringLiteral("7000");
        m_DatabaseManager->setSetting(kSettingFrpsBindPort, bindPort);
    }
    QString token = m_DatabaseManager->getSetting(kSettingFrpsToken);
    if (token.trimmed().isEmpty())
    {
        token = randomHexToken(16);
        m_DatabaseManager->setSetting(kSettingFrpsToken, token);
    }
    m_Ui->frpsPortEdit->setText(bindPort);
    m_Ui->frpsTokenEdit->setText(token);
    // frps.exe 路径：无自定义选择时自动指向内置的 <程序目录>/frp/frps.exe
    QString frpsPath = m_FrpsManager->frpsPath();
    if (frpsPath.isEmpty() && QFile::exists(FrpUpdater::frpsPath()))
    {
        frpsPath = FrpUpdater::frpsPath();
        m_FrpsManager->setFrpsPath(frpsPath);
    }
    m_Ui->frpsPathEdit->setText(frpsPath);
    // 仪表盘（frps webServer）端口：随数据库恢复，默认 7500
    m_Ui->webPortEdit->setText(m_DatabaseManager->getSetting(kSettingFrpsWebPort,
                                                             QStringLiteral("7500")));
    updateFrpsStatusUi();

    // 数据库切换后重置流量采样基准
    m_TrafficMonitor->clearBaselines();

    // 面板服务端口 = 用户管理页设置的公网端口（客户端登录端口）
    m_Ui->panelPortEdit->setText(m_DatabaseManager->getSetting(kSettingPublicPort));
    updatePanelServiceUi();
    syncPanelServiceWithDb();

    onRefreshUserComboBox();
    updateControlsEnabled();
}

void ServerTunnelPage::onRefreshUserComboBox()
{
    const int previousUserId = m_Ui->userComboBox->currentData().toInt();
    m_Ui->userComboBox->blockSignals(true);
    m_Ui->userComboBox->clear();

    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        QString displayName = user.username;
        if (isExpired(user.expireAt))
        {
            displayName = tr("[已过期] ") + displayName;
        }
        m_Ui->userComboBox->addItem(displayName, user.id);
    }
    const int index = m_Ui->userComboBox->findData(previousUserId);
    m_Ui->userComboBox->setCurrentIndex(index >= 0 ? index : 0);
    m_Ui->userComboBox->blockSignals(false);
    onCurrentUserChanged();
}

void ServerTunnelPage::onCurrentUserChanged()
{
    // 空下拉框（无用户）时 user id 视为 -1，避免无效值 0 通过 < 0 守卫
    m_CurrentUserId = (m_Ui->userComboBox->currentIndex() >= 0)
                          ? m_Ui->userComboBox->currentData().toInt()
                          : -1;
    loadQuotaToUi();
    refreshTunnelTable();
    updateControlsEnabled();
    saveServerUiState();
}

void ServerTunnelPage::saveServerUiState()
{
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                       QSettings::IniFormat);
    settings.beginGroup(kServerStateSection);
    settings.setValue(QStringLiteral("dbName"), m_Ui->dbComboBox->currentText());
    settings.setValue(QStringLiteral("userId"), m_CurrentUserId);
    settings.endGroup();
    settings.sync();
}

void ServerTunnelPage::applyRememberedUiState()
{
    if (m_RememberedDbName.isEmpty() && m_RememberedUserId <= 0)
    {
        return;
    }
    if (!m_RememberedDbName.isEmpty())
    {
        const int dbIndex = m_Ui->dbComboBox->findText(m_RememberedDbName);
        if (dbIndex >= 0)
        {
            // 触发 onCurrentDbChanged 整链（打开数据库、刷新用户等）
            m_Ui->dbComboBox->setCurrentIndex(dbIndex);
        }
    }
    if (m_RememberedUserId > 0)
    {
        const int userIndex = m_Ui->userComboBox->findData(m_RememberedUserId);
        if (userIndex >= 0)
        {
            m_Ui->userComboBox->setCurrentIndex(userIndex);
        }
    }
}

void ServerTunnelPage::onSearchTunnels()
{
    refreshTunnelTable();
}

void ServerTunnelPage::onAddTunnel()
{
    if (m_CurrentUserId <= 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, tr("提示"),
                                   tr("请先在用户管理中为该数据库创建用户，再选择用户添加隧道"),
                                   2500, this);
        return;
    }
    // 防御：用户可能已在其他页面被删除，先校验再操作
    bool userExists = false;
    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        if (user.id == m_CurrentUserId)
        {
            userExists = true;
            break;
        }
    }
    if (!userExists)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("所选用户已被删除，列表已刷新"), 2500, this);
        onRefreshUserComboBox();
        return;
    }

    TunnelEditDialog dialog(false, QStringLiteral("server"), this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    QString errorMessage;
    if (!m_DatabaseManager->addTunnel(m_CurrentUserId, dialog.name(), dialog.protocol(),
                                      dialog.remotePort(), dialog.localIp(), dialog.localPort(),
                                      dialog.customDomain(), dialog.isEnabled(), dialog.remark(),
                                      &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             errorMessage, 2500, this);
        return;
    }
    refreshTunnelTable();
    applyFrpsConfig(true);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("提示"),
                           tr("隧道已添加"), 2000, this);
}

void ServerTunnelPage::onEditTunnel()
{
    const int id = selectedTunnelId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, tr("提示"),
                                   tr("请先在列表中选择要修改的隧道"), 2000, this);
        return;
    }

    const QList<DatabaseManager::TunnelInfo> tunnels = m_DatabaseManager->queryTunnels(m_CurrentUserId);
    const DatabaseManager::TunnelInfo* target = nullptr;
    for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
    {
        if (tunnel.id == id)
        {
            target = &tunnel;
            break;
        }
    }
    if (!target)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("未找到该隧道"), 2000, this);
        return;
    }

    TunnelEditDialog dialog(true, QStringLiteral("server"), this);
    dialog.setName(target->name);
    dialog.setProtocol(target->protocol);
    dialog.setRemotePort(target->remotePort);
    dialog.setLocalIp(target->localIp);
    dialog.setLocalPort(target->localPort);
    dialog.setCustomDomain(target->customDomain);
    dialog.setIsEnabled(target->isEnabled);
    dialog.setRemark(target->remark);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QString errorMessage;
    if (!m_DatabaseManager->updateTunnel(id, dialog.name(), dialog.protocol(),
                                         dialog.remotePort(), dialog.localIp(), dialog.localPort(),
                                         dialog.customDomain(), dialog.isEnabled(), dialog.remark(),
                                         &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             errorMessage, 2500, this);
        return;
    }
    refreshTunnelTable();
    applyFrpsConfig(true);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("提示"),
                           tr("隧道已更新"), 2000, this);
}

void ServerTunnelPage::onDeleteTunnel()
{
    const int id = selectedTunnelId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, tr("提示"),
                                   tr("请先在列表中选择要删除的隧道"), 2000, this);
        return;
    }
    showConfirmDialog(
        tr("确认删除"),
        tr("确定要删除该隧道吗？"),
        tr("删除"),
        [this, id]() {
            if (!m_DatabaseManager->deleteTunnel(id))
            {
                ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                                     tr("删除隧道失败"), 2000, this);
                return;
            }
            refreshTunnelTable();
            applyFrpsConfig(true);
            ElaMessageBar::success(ElaMessageBarType::TopRight, tr("提示"),
                                   tr("隧道已删除"), 2000, this);
        });
}

void ServerTunnelPage::onBrowseFrps()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("选择 frps.exe"), QString(),
        tr("frps (*.exe);;所有文件 (*)"));
    if (path.isEmpty())
    {
        return;
    }
    m_FrpsManager->setFrpsPath(path);
    m_Ui->frpsPathEdit->setText(path);
}

void ServerTunnelPage::onToggleFrps()
{
    if (m_FrpsManager->isRunning())
    {
        m_FrpsManager->stop();
        appendLog(tr("[%1] frps 已停止")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        return;
    }

    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, tr("提示"),
                                   tr("请先选择数据库"), 2000, this);
        return;
    }

    // 保存端口与 Token 到当前数据库
    const QString bindPort = m_Ui->frpsPortEdit->text().trimmed();
    const QString token = m_Ui->frpsTokenEdit->text().trimmed();
    const QString webPort = m_Ui->webPortEdit->text().trimmed();
    bool portOk = false;
    const int portValue = bindPort.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("绑定端口必须是 1-65535 的整数"), 2000, this);
        return;
    }
    bool webPortOk = false;
    const int webPortValue = webPort.toInt(&webPortOk);
    if (!webPortOk || webPortValue < 1 || webPortValue > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("仪表盘端口必须是 1-65535 的整数"), 2000, this);
        return;
    }
    if (token.isEmpty())
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("Token 不能为空"), 2000, this);
        return;
    }
    m_DatabaseManager->setSetting(kSettingFrpsBindPort, bindPort);
    m_DatabaseManager->setSetting(kSettingFrpsToken, token);
    m_DatabaseManager->setSetting(kSettingFrpsWebPort, webPort);

    // 启动前预检端口占用（避免 frps 报 "Only one usage of each socket address"）
    QString portError;
    if (!checkFrpsPortsAvailable(portValue, webPortValue, &portError))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("端口被占用"),
                             portError, 5000, this);
        return;
    }

    applyFrpsConfig(false);
    QString errorMessage;
    if (!m_FrpsManager->start(frpsConfigPath(), &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             errorMessage, 3000, this);
        appendLog(tr("[%1] frps 启动失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    appendLog(tr("[%1] frps 已启动 (配置: %2)")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), frpsConfigPath()));
}

bool ServerTunnelPage::checkFrpsPortsAvailable(int bindPort, int webPort, QString* errorMessage) const
{
    if (!PortChecker::isPortFree(static_cast<quint16>(bindPort), QStringLiteral("tcp")))
    {
        if (errorMessage)
        {
            *errorMessage = tr(
                "frps 绑定端口 %1 已被占用（可能是上一次程序异常退出后残留的 frps 进程，"
                "或其他程序占用）。\n请结束占用该端口的进程，或修改上方端口后重试。")
                                .arg(bindPort);
        }
        return false;
    }
    if (webPort > 0 && !PortChecker::isPortFree(static_cast<quint16>(webPort), QStringLiteral("tcp")))
    {
        if (errorMessage)
        {
            *errorMessage = tr(
                "frps 仪表盘端口 %1 已被占用（可能是上一次程序异常退出后残留的 frps 进程，"
                "或其他程序占用）。\n请结束占用该端口的进程，或修改 Web 端口后重试。")
                                .arg(webPort);
        }
        return false;
    }
    return true;
}

void ServerTunnelPage::onTogglePanelService()
{
    if (m_PanelApiServer->isRunning())
    {
        m_PanelApiServer->stop();
        m_PanelServiceDbName.clear();
        m_PanelServicePort = 0;
        appendLog(tr("[%1] 面板服务已停止")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        return;
    }
    const QString portText = m_Ui->panelPortEdit->text().trimmed();
    bool portOk = false;
    const int portValue = portText.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("请先在用户管理页设置有效的公网端口（客户端登录端口）"), 3000, this);
        return;
    }
    // 同步公网端口设置（与用户管理页保持一致）
    m_DatabaseManager->setSetting(kSettingPublicPort, portText);

    QString errorMessage;
    if (!m_PanelApiServer->start(static_cast<quint16>(portValue), &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             errorMessage, 3000, this);
        appendLog(tr("[%1] 面板服务启动失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    m_PanelServiceDbName = m_Ui->dbComboBox->currentText();
    m_PanelServicePort = portValue;
    appendLog(tr("[%1] 面板服务已启动，监听端口 %2（客户端在此端口登录）")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")))
                  .arg(portValue));
}

void ServerTunnelPage::updatePanelServiceUi()
{
    const bool running = m_PanelApiServer->isRunning();
    m_Ui->panelServiceButton->setText(running ? tr("停止服务") : tr("启动服务"));
    m_Ui->panelServiceStatusLabel->setText(running
                                               ? tr("运行中 (端口 %1)").arg(m_PanelApiServer->port())
                                               : tr("未运行"));
}

void ServerTunnelPage::syncPanelServiceWithDb()
{
    if (!m_PanelApiServer->isRunning())
    {
        return;
    }
    const QString dbName = m_Ui->dbComboBox->currentText();
    const QString portText = m_Ui->panelPortEdit->text().trimmed();
    bool portOk = false;
    const int portValue = portText.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        m_PanelApiServer->stop();
        m_PanelServiceDbName.clear();
        m_PanelServicePort = 0;
        appendLog(tr("[%1] 面板服务已停止（新数据库未设置公网端口）")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        return;
    }
    // 数据库与端口都没变（如页面刷新、切换页签）时绝不重启，避免在线客户端掉线
    if (m_PanelServiceDbName == dbName && m_PanelServicePort == portValue)
    {
        return;
    }
    m_PanelApiServer->stop();
    QString errorMessage;
    if (!m_PanelApiServer->start(static_cast<quint16>(portValue), &errorMessage))
    {
        m_PanelServiceDbName.clear();
        m_PanelServicePort = 0;
        appendLog(tr("[%1] 面板服务重启失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    m_PanelServiceDbName = dbName;
    m_PanelServicePort = portValue;
    appendLog(tr("[%1] 面板服务已切换到新数据库端口 %2")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")))
                  .arg(portValue));
}

void ServerTunnelPage::updateGlobalOverview()
{
    GlobalInformation& g = g_GlobalInformation;
    g.serverDbOpen = m_DatabaseManager->isOpen();
    g.serverDbName = g.serverDbOpen ? m_DatabaseManager->currentDatabaseName() : QString();
    g.frpsRunning = m_FrpsManager->isRunning();
    g.frpsBindPort = m_Ui->frpsPortEdit->text().trimmed().toInt();
    // 仪表盘端口：界面输入优先，其次数据库设置，再退到 frp 惯例 7500
    const QString webPortText = m_Ui->webPortEdit->text().trimmed();
    const int webPortValue = webPortText.isEmpty()
                                 ? m_DatabaseManager->getSetting(kSettingFrpsWebPort,
                                                                 QStringLiteral("7500")).toInt()
                                 : webPortText.toInt();
    g.frpsWebPort = webPortValue;
    g.panelRunning = m_PanelApiServer->isRunning();
    g.panelPort = g.panelRunning ? m_PanelApiServer->port() : 0;

    if (g.serverDbOpen)
    {
        g.usersCount = m_DatabaseManager->countUsers(false);
        g.tunnelsCount = m_DatabaseManager->countTunnels(false);
        g.tunnelsEnabledCount = m_DatabaseManager->countTunnels(true);
        const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        const DatabaseManager::TrafficSummary todaySummary =
            m_DatabaseManager->queryTrafficRangeTotal(today, today);
        g.todayBytesIn = todaySummary.bytesIn;
        g.todayBytesOut = todaySummary.bytesOut;
        const DatabaseManager::TrafficSummary totalSummary =
            m_DatabaseManager->queryTrafficRangeTotal(QString(), QString());
        g.totalBytesIn = totalSummary.bytesIn;
        g.totalBytesOut = totalSummary.bytesOut;
    }
    else
    {
        g.usersCount = 0;
        g.tunnelsCount = 0;
        g.tunnelsEnabledCount = 0;
        g.todayBytesIn = 0;
        g.todayBytesOut = 0;
        g.totalBytesIn = 0;
        g.totalBytesOut = 0;
    }
}

void ServerTunnelPage::onClearLog()
{
    m_Ui->logTextEdit->clear();
}

void ServerTunnelPage::onSaveQuota()
{
    if (m_CurrentUserId <= 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, tr("提示"),
                                   tr("请先选择用户"), 2000, this);
        return;
    }
    const int remoteMin = m_Ui->remoteMinEdit->text().trimmed().toInt();
    const int remoteMax = m_Ui->remoteMaxEdit->text().trimmed().toInt();
    const int localMin = m_Ui->localMinEdit->text().trimmed().toInt();
    const int localMax = m_Ui->localMaxEdit->text().trimmed().toInt();
    const int maxCount = m_Ui->maxPortCountEdit->text().trimmed().toInt();
    if (remoteMin < 1 || remoteMax > 65535 || remoteMin > remoteMax)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("远端端口范围无效（1-65535 且最小值不大于最大值）"), 2500, this);
        return;
    }
    if (localMin < 1 || localMax > 65535 || localMin > localMax)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("本地端口范围无效（1-65535 且最小值不大于最大值）"), 2500, this);
        return;
    }
    if (maxCount < 1 || maxCount > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, tr("提示"),
                               tr("最大端口数必须是 1-65535 的整数"), 2500, this);
        return;
    }

    if (!m_DatabaseManager->setUserQuota(m_CurrentUserId, remoteMin, remoteMax,
                                         localMin, localMax, maxCount))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             tr("保存配额失败"), 2000, this);
        return;
    }
    applyFrpsConfig(true);
    ElaMessageBar::success(ElaMessageBarType::TopRight, tr("提示"),
                           tr("端口配额已保存（远端 %1-%2，本地 %3-%4，最多 %5 个端口）")
                               .arg(remoteMin)
                               .arg(remoteMax)
                               .arg(localMin)
                               .arg(localMax)
                               .arg(maxCount),
                           3000, this);
}

void ServerTunnelPage::refreshTunnelTable()
{
    // 清理旧的开关控件（setIndexWidget 会删除原控件）
    for (int row = 0; row < m_TunnelModel->rowCount(); ++row)
    {
        m_Ui->tunnelTableView->setIndexWidget(m_TunnelModel->index(row, 0), nullptr);
    }
    m_TunnelModel->setRowCount(0);

    if (m_CurrentUserId < 0)
    {
        return;
    }

    const bool frpsRunning = m_FrpsManager->isRunning();
    const QList<DatabaseManager::TunnelInfo> tunnels =
        m_DatabaseManager->queryTunnels(m_CurrentUserId, m_Ui->searchLineEdit->text());
    for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
    {
        const int row = m_TunnelModel->rowCount();
        m_TunnelModel->insertRow(row);

        QStandardItem* switchItem = new QStandardItem();
        switchItem->setData(tunnel.id, Qt::UserRole);
        switchItem->setTextAlignment(Qt::AlignCenter);
        // 显式尺寸提示，保证开关列足够宽（否则开关被裁剪、无法点击）
        switchItem->setSizeHint(QSize(56, 26));
        m_TunnelModel->setItem(row, 0, switchItem);

        QStandardItem* nameItem = new QStandardItem(tunnel.name);
        nameItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 1, nameItem);

        QStandardItem* protocolItem = new QStandardItem(tunnel.protocol);
        protocolItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 2, protocolItem);

        QStandardItem* portItem = new QStandardItem(
            (tunnel.protocol == QStringLiteral("http") || tunnel.protocol == QStringLiteral("https"))
                ? QStringLiteral("-")
                : QString::number(tunnel.remotePort));
        portItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 3, portItem);

        QString targetText = QStringLiteral("%1:%2").arg(tunnel.localIp).arg(tunnel.localPort);
        if (!tunnel.customDomain.trimmed().isEmpty())
        {
            targetText += QStringLiteral(" / ") + tunnel.customDomain.trimmed();
        }
        QStandardItem* targetItem = new QStandardItem(targetText);
        targetItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 4, targetItem);

        QString statusText;
        if (!tunnel.isEnabled)
        {
            statusText = tr("已禁用");
        }
        else if (frpsRunning)
        {
            statusText = tr("运行中");
        }
        else
        {
            statusText = tr("未运行");
        }
        QStandardItem* statusItem = new QStandardItem(statusText);
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 5, statusItem);

        QStandardItem* remarkItem = new QStandardItem(tunnel.remark);
        remarkItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 6, remarkItem);

        // 行内开关：切换隧道启用状态，立即重新生成配置并热重启 frps
        ElaToggleSwitch* toggleSwitch = new ElaToggleSwitch(m_Ui->tunnelTableView);
        // 修复 ElaToggleSwitch 初始绘制：私有成员 _circleCenterX 初始为 0，
        // 首次绘制旋钮会停在最左端；连续两次 setIsToggled 驱动旋钮动画到正确位置
        // （此时尚未 connect，不会误触发信号）
        toggleSwitch->setIsToggled(!tunnel.isEnabled);
        toggleSwitch->setIsToggled(tunnel.isEnabled);
        m_Ui->tunnelTableView->setIndexWidget(m_TunnelModel->index(row, 0), toggleSwitch);
        connect(toggleSwitch, &ElaToggleSwitch::toggled, this, [this, tunnelId = tunnel.id](bool checked) {
            if (!m_DatabaseManager->setTunnelEnabled(tunnelId, checked))
            {
                ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                                     tr("更新隧道状态失败"), 2000, this);
                refreshTunnelTable();
                return;
            }
            applyFrpsConfig(true);
            // 局部更新该行的运行状况文本，避免整体重建中断开关的点击事件
            const QString newStatus = checked
                                          ? (m_FrpsManager->isRunning() ? tr("运行中")
                                                                         : tr("未运行"))
                                          : tr("已禁用");
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

void ServerTunnelPage::updateControlsEnabled()
{
    const bool hasDatabase = !m_Ui->dbComboBox->currentText().isEmpty();
    m_Ui->userComboBox->setEnabled(hasDatabase);
    m_Ui->frpsPortEdit->setEnabled(hasDatabase);
    m_Ui->frpsTokenEdit->setEnabled(hasDatabase);
    m_Ui->startButton->setEnabled(hasDatabase);
    const bool hasUser = m_CurrentUserId > 0;
    m_Ui->remoteMinEdit->setEnabled(hasUser);
    m_Ui->remoteMaxEdit->setEnabled(hasUser);
    m_Ui->localMinEdit->setEnabled(hasUser);
    m_Ui->localMaxEdit->setEnabled(hasUser);
    m_Ui->maxPortCountEdit->setEnabled(hasUser);
    m_Ui->saveQuotaButton->setEnabled(hasUser);
    m_Ui->panelPortEdit->setEnabled(hasDatabase);
    m_Ui->panelServiceButton->setEnabled(hasDatabase);
    m_Ui->searchLineEdit->setEnabled(hasUser);
    m_Ui->searchButton->setEnabled(hasUser);
    m_Ui->addTunnelButton->setEnabled(hasUser);
    m_Ui->editTunnelButton->setEnabled(hasUser);
    m_Ui->deleteTunnelButton->setEnabled(hasUser);
}

void ServerTunnelPage::loadQuotaToUi()
{
    if (m_CurrentUserId <= 0)
    {
        m_Ui->remoteMinEdit->clear();
        m_Ui->remoteMaxEdit->clear();
        m_Ui->localMinEdit->clear();
        m_Ui->localMaxEdit->clear();
        m_Ui->maxPortCountEdit->clear();
        return;
    }
    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        if (user.id == m_CurrentUserId)
        {
            m_Ui->remoteMinEdit->setText(QString::number(user.remotePortMin));
            m_Ui->remoteMaxEdit->setText(QString::number(user.remotePortMax));
            m_Ui->localMinEdit->setText(QString::number(user.localPortMin));
            m_Ui->localMaxEdit->setText(QString::number(user.localPortMax));
            m_Ui->maxPortCountEdit->setText(QString::number(user.maxPortCount));
            return;
        }
    }
    m_Ui->remoteMinEdit->clear();
    m_Ui->remoteMaxEdit->clear();
    m_Ui->localMinEdit->clear();
    m_Ui->localMaxEdit->clear();
    m_Ui->maxPortCountEdit->clear();
}

int ServerTunnelPage::selectedTunnelId() const
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

void ServerTunnelPage::appendLog(const QString& text)
{
    m_Ui->logTextEdit->appendPlainText(text);
}

void ServerTunnelPage::updateFrpsStatusUi()
{
    const bool running = m_FrpsManager->isRunning();
    m_Ui->startButton->setText(running ? tr("停止") : tr("启动"));
    m_Ui->frpsStatusLabel->setText(running ? tr("运行中") : tr("未运行"));
    // 状态灯：运行中=绿，未运行=灰
    m_Ui->frpsStatusLight->setColor(running ? QColor(0x4C, 0xAF, 0x50)
                                            : QColor(0x9E, 0x9E, 0x9E));
}

void ServerTunnelPage::updateTunnelStatusColumn()
{
    // frps 启停只影响"运行中/未运行"，已禁用的隧道保持不动
    const bool frpsRunning = m_FrpsManager->isRunning();
    for (int row = 0; row < m_TunnelModel->rowCount(); ++row)
    {
        QStandardItem* statusItem = m_TunnelModel->item(row, 5);
        if (!statusItem)
        {
            continue;
        }
        const QString text = statusItem->text();
        if (text == tr("已禁用"))
        {
            continue;
        }
        statusItem->setText(frpsRunning ? tr("运行中") : tr("未运行"));
    }
}

void ServerTunnelPage::applyFrpsConfig(bool restartIfRunning)
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        return;
    }

    const QString bindPort = m_Ui->frpsPortEdit->text().trimmed();
    const QString token = m_Ui->frpsTokenEdit->text().trimmed();
    if (bindPort.isEmpty() || token.isEmpty())
    {
        return;
    }
    // 同步写入数据库：保证客户端登录时下发的 frpsBindPort 与实际运行端口永远一致，
    // 避免"frps 已按 UI 端口运行、数据库仍是旧值"导致 frpc 连错端口
    m_DatabaseManager->setSetting(kSettingFrpsBindPort, bindPort);
    m_DatabaseManager->setSetting(kSettingFrpsToken, token);

    // 仪表盘（流量监控数据源）：端口优先取界面输入，账号/密码首次自动生成
    QString webPort = m_Ui->webPortEdit->text().trimmed();
    if (webPort.isEmpty())
    {
        webPort = m_DatabaseManager->getSetting(kSettingFrpsWebPort);
    }
    if (webPort.trimmed().isEmpty())
    {
        webPort = QStringLiteral("7500");
    }
    m_DatabaseManager->setSetting(kSettingFrpsWebPort, webPort);
    QString webUser = m_DatabaseManager->getSetting(kSettingFrpsWebUser);
    if (webUser.trimmed().isEmpty())
    {
        webUser = QStringLiteral("admin");
        m_DatabaseManager->setSetting(kSettingFrpsWebUser, webUser);
    }
    QString webPassword = m_DatabaseManager->getSetting(kSettingFrpsWebPassword);
    if (webPassword.trimmed().isEmpty())
    {
        webPassword = randomHexToken(16);
        m_DatabaseManager->setSetting(kSettingFrpsWebPassword, webPassword);
    }
    m_Ui->webPortEdit->setText(webPort);

    QString errorMessage;
    if (!FrpsManager::generateConfig(frpsConfigPath(), bindPort.toUShort(), token,
                                     collectPortRanges(), webPort.toUShort(), webUser, webPassword,
                                     &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                             errorMessage, 2500, this);
        return;
    }
    appendLog(tr("[%1] frps 配置已重新生成 (%2)")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), frpsConfigPath()));

    if (restartIfRunning && m_FrpsManager->isRunning())
    {
        // 重启前预检端口占用（可能是上次异常退出残留的 frps 进程占着端口）
        QString portError;
        if (!checkFrpsPortsAvailable(bindPort.toInt(), webPort.toInt(), &portError))
        {
            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("端口被占用"),
                                 portError, 5000, this);
            appendLog(tr("[%1] frps 重启被拒绝: %2")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), portError));
            return;
        }
        m_FrpsManager->stop();
        QString startError;
        if (!m_FrpsManager->start(frpsConfigPath(), &startError))
        {
            ElaMessageBar::error(ElaMessageBarType::TopRight, tr("提示"),
                                 startError, 3000, this);
            appendLog(tr("[%1] frps 重启失败: %2")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), startError));
        }
        else
        {
            appendLog(tr("[%1] frps 已重启")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        }
    }
}

QString ServerTunnelPage::frpsConfigPath() const
{
    QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.endsWith(QStringLiteral(".db")))
    {
        fileName.chop(3);
    }
    return DatabaseManager::dataDirectory() + QLatin1Char('/') + fileName + QStringLiteral(".frps.toml");
}

QList<QPair<quint16, quint16>> ServerTunnelPage::collectPortRanges() const
{
    // 汇总所有用户的远端端口范围（服务端界定的配额），写入 frps allowPorts 白名单
    QList<QPair<quint16, quint16>> ranges;
    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        const QPair<quint16, quint16> range(static_cast<quint16>(user.remotePortMin),
                                            static_cast<quint16>(user.remotePortMax));
        if (range.first <= range.second && !ranges.contains(range))
        {
            ranges.append(range);
        }
    }
    std::sort(ranges.begin(), ranges.end());
    return ranges;
}

void ServerTunnelPage::showConfirmDialog(const QString& title, const QString& content,
                                         const QString& confirmText, std::function<void()> onConfirm)
{
    ElaContentDialog* dialog = new ElaContentDialog(this);
    dialog->setLeftButtonText(tr("取消"));
    dialog->setMiddleButtonText(QString());
    dialog->setRightButtonText(confirmText);

    QWidget* centralWidget = new QWidget(dialog);
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(15, 14, 15, 8);
    ElaText* titleText = new ElaText(title, centralWidget);
    titleText->setTextStyle(ElaTextType::Body);
    titleText->setTextPixelSize(15);
    ElaText* contentText = new ElaText(content, centralWidget);
    contentText->setTextStyle(ElaTextType::Body);
    contentText->setTextPixelSize(13);
    contentText->setWordWrap(true);
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
    connect(dialog, &ElaContentDialog::rightButtonClicked, this, [dialog, onConfirm]() {
        if (onConfirm)
        {
            onConfirm();
        }
    });
    dialog->exec();
    dialog->deleteLater();
}
