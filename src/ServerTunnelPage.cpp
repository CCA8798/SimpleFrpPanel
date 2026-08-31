#include "ServerTunnelPage.h"

#include <QDate>
#include <QFileDialog>
#include <QHeaderView>
#include <QIntValidator>
#include <QRandomGenerator>
#include <QStandardItemModel>
#include <QTime>
#include <QVBoxLayout>

#include "DatabaseManager.h"
#include "ElaContentDialog.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToggleSwitch.h"
#include "FrpsManager.h"
#include "TunnelEditDialog.h"
#include "ui_ServerTunnelPage.h"

namespace {
const QString kSettingFrpsBindPort = QStringLiteral("frps_bind_port");
const QString kSettingFrpsToken = QStringLiteral("frps_token");

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
    , m_FrpsManager(new FrpsManager(this))
    , m_TunnelModel(new QStandardItemModel(this))
{
    m_Ui->setupUi(this);

    // 布局：顶部固定紧凑，中部自动扩展，日志区固定高度
    m_Ui->topFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_Ui->logFrame->setFixedHeight(150);

    // 端口输入校验
    m_Ui->frpsPortEdit->setValidator(new QIntValidator(1, 65535, this));

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

    // 日志面板限制行数
    m_Ui->logTextEdit->setMaximumBlockCount(2000);

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

    connect(m_FrpsManager, &FrpsManager::runningChanged, this, [this](bool) {
        updateFrpsStatusUi();
        refreshTunnelTable();
    });
    connect(m_FrpsManager, &FrpsManager::logMessage, this, &ServerTunnelPage::appendLog);

    onRefreshDbComboBox();
}

ServerTunnelPage::~ServerTunnelPage()
{
    delete m_Ui;
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
        m_CurrentUserId = -1;
        m_Ui->userComboBox->clear();
        m_Ui->frpsPathEdit->clear();
        m_Ui->frpsPortEdit->clear();
        m_Ui->frpsTokenEdit->clear();
        m_Ui->frpsStatusLabel->setText(QStringLiteral("未运行"));
        refreshTunnelTable();
        updateControlsEnabled();
        return;
    }

    if (m_DatabaseManager->currentDatabaseName() != fileName
        && !m_DatabaseManager->openDatabase(fileName))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             QStringLiteral("打开数据库 %1 失败").arg(fileName), 2000, this);
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
    m_Ui->frpsPathEdit->setText(m_FrpsManager->frpsPath());
    updateFrpsStatusUi();

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
            displayName = QStringLiteral("[已过期] ") + displayName;
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
    m_CurrentUserId = m_Ui->userComboBox->currentData().toInt();
    refreshTunnelTable();
}

void ServerTunnelPage::onSearchTunnels()
{
    refreshTunnelTable();
}

void ServerTunnelPage::onAddTunnel()
{
    if (m_CurrentUserId < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先选择数据库和用户"), 2000, this);
        return;
    }
    TunnelEditDialog dialog(false, this);
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
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 2500, this);
        return;
    }
    refreshTunnelTable();
    applyFrpsConfig(true);
    ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("隧道已添加"), 2000, this);
}

void ServerTunnelPage::onEditTunnel()
{
    const int id = selectedTunnelId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先在列表中选择要修改的隧道"), 2000, this);
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
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("未找到该隧道"), 2000, this);
        return;
    }

    TunnelEditDialog dialog(true, this);
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
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 2500, this);
        return;
    }
    refreshTunnelTable();
    applyFrpsConfig(true);
    ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("隧道已更新"), 2000, this);
}

void ServerTunnelPage::onDeleteTunnel()
{
    const int id = selectedTunnelId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先在列表中选择要删除的隧道"), 2000, this);
        return;
    }
    showConfirmDialog(
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除该隧道吗？"),
        QStringLiteral("删除"),
        [this, id]() {
            if (!m_DatabaseManager->deleteTunnel(id))
            {
                ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                     QStringLiteral("删除隧道失败"), 2000, this);
                return;
            }
            refreshTunnelTable();
            applyFrpsConfig(true);
            ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("隧道已删除"), 2000, this);
        });
}

void ServerTunnelPage::onBrowseFrps()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 frps.exe"), QString(),
        QStringLiteral("frps (*.exe);;所有文件 (*)"));
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
        appendLog(QStringLiteral("[%1] frps 已停止")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
        return;
    }

    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先选择数据库"), 2000, this);
        return;
    }

    // 保存端口与 Token 到当前数据库
    const QString bindPort = m_Ui->frpsPortEdit->text().trimmed();
    const QString token = m_Ui->frpsTokenEdit->text().trimmed();
    bool portOk = false;
    const int portValue = bindPort.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("绑定端口必须是 1-65535 的整数"), 2000, this);
        return;
    }
    if (token.isEmpty())
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("Token 不能为空"), 2000, this);
        return;
    }
    m_DatabaseManager->setSetting(kSettingFrpsBindPort, bindPort);
    m_DatabaseManager->setSetting(kSettingFrpsToken, token);

    applyFrpsConfig(false);
    QString errorMessage;
    if (!m_FrpsManager->start(frpsConfigPath(), &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 3000, this);
        appendLog(QStringLiteral("[%1] frps 启动失败: %2")
                      .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), errorMessage));
        return;
    }
    appendLog(QStringLiteral("[%1] frps 已启动 (配置: %2)")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), frpsConfigPath()));
}

void ServerTunnelPage::onClearLog()
{
    m_Ui->logTextEdit->clear();
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
            statusText = QStringLiteral("已禁用");
        }
        else if (frpsRunning)
        {
            statusText = QStringLiteral("运行中");
        }
        else
        {
            statusText = QStringLiteral("未运行");
        }
        QStandardItem* statusItem = new QStandardItem(statusText);
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 5, statusItem);

        QStandardItem* remarkItem = new QStandardItem(tunnel.remark);
        remarkItem->setTextAlignment(Qt::AlignCenter);
        m_TunnelModel->setItem(row, 6, remarkItem);

        // 行内开关：切换隧道启用状态，立即重新生成配置并热重启 frps
        ElaToggleSwitch* toggleSwitch = new ElaToggleSwitch(m_Ui->tunnelTableView);
        toggleSwitch->setIsToggled(tunnel.isEnabled);
        m_Ui->tunnelTableView->setIndexWidget(m_TunnelModel->index(row, 0), toggleSwitch);
        connect(toggleSwitch, &ElaToggleSwitch::toggled, this, [this, tunnelId = tunnel.id](bool checked) {
            if (!m_DatabaseManager->setTunnelEnabled(tunnelId, checked))
            {
                ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                     QStringLiteral("更新隧道状态失败"), 2000, this);
            }
            else
            {
                applyFrpsConfig(true);
            }
            refreshTunnelTable();
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
    const bool hasUser = m_CurrentUserId >= 0;
    m_Ui->searchLineEdit->setEnabled(hasUser);
    m_Ui->searchButton->setEnabled(hasUser);
    m_Ui->addTunnelButton->setEnabled(hasUser);
    m_Ui->editTunnelButton->setEnabled(hasUser);
    m_Ui->deleteTunnelButton->setEnabled(hasUser);
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
    m_Ui->startButton->setText(running ? QStringLiteral("停止") : QStringLiteral("启动"));
    m_Ui->frpsStatusLabel->setText(running ? QStringLiteral("运行中") : QStringLiteral("未运行"));
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

    QString errorMessage;
    if (!FrpsManager::generateConfig(frpsConfigPath(), bindPort.toUShort(), token,
                                     collectAllowedPorts(), &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 2500, this);
        return;
    }
    appendLog(QStringLiteral("[%1] frps 配置已重新生成 (%2)")
                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), frpsConfigPath()));

    if (restartIfRunning && m_FrpsManager->isRunning())
    {
        m_FrpsManager->stop();
        QString startError;
        if (!m_FrpsManager->start(frpsConfigPath(), &startError))
        {
            ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                 startError, 3000, this);
            appendLog(QStringLiteral("[%1] frps 重启失败: %2")
                          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), startError));
        }
        else
        {
            appendLog(QStringLiteral("[%1] frps 已重启")
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

QList<quint16> ServerTunnelPage::collectAllowedPorts() const
{
    QList<quint16> ports;
    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        const QList<DatabaseManager::TunnelInfo> tunnels = m_DatabaseManager->queryTunnels(user.id);
        for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
        {
            if (tunnel.isEnabled && tunnel.remotePort > 0 && !ports.contains(tunnel.remotePort))
            {
                ports.append(tunnel.remotePort);
            }
        }
    }
    std::sort(ports.begin(), ports.end());
    return ports;
}

void ServerTunnelPage::showConfirmDialog(const QString& title, const QString& content,
                                         const QString& confirmText, std::function<void()> onConfirm)
{
    ElaContentDialog* dialog = new ElaContentDialog(this);
    dialog->setLeftButtonText(QStringLiteral("取消"));
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
