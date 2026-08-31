#include "ServerUserPage.h"

#include <QHeaderView>
#include <QIntValidator>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "DatabaseManager.h"
#include "ElaContentDialog.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "UserEditDialog.h"
#include "ui_ServerUserPage.h"

namespace {
const QString kSettingPublicIp = QStringLiteral("public_ip");
const QString kSettingPublicPort = QStringLiteral("public_port");

bool isValidIpv4(const QString& ip)
{
    static const QRegularExpression pattern(
        QStringLiteral("^((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\\.){3}"
                       "(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])$"));
    return pattern.match(ip).hasMatch();
}
} // namespace

ServerUserPage::ServerUserPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::ServerUserPage())
    , m_DatabaseManager(new DatabaseManager(this))
    , m_UserModel(new QStandardItemModel(this))
{
    m_Ui->setupUi(this);

    // 布局：上部（数据库部分）固定高度、紧凑，不随窗口拉伸；
    // 下部（用户部分）随窗口尺寸自动扩展（其余空间全部归下部）
    m_Ui->dbFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // 端口输入校验
    m_Ui->portLineEdit->setValidator(new QIntValidator(1, 65535, this));

    // 用户表模型：ID / 用户名 / 备注 / 状态 / 到期时间 / 创建时间
    m_UserModel->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("ID") << QStringLiteral("用户名")
                      << QStringLiteral("备注") << QStringLiteral("状态")
                      << QStringLiteral("到期时间") << QStringLiteral("创建时间"));
    m_Ui->userTableView->setModel(m_UserModel);
    m_Ui->userTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_Ui->userTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_Ui->userTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_Ui->userTableView->verticalHeader()->setVisible(false);
    m_Ui->userTableView->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    connect(m_Ui->newDbButton, &QPushButton::clicked, this, &ServerUserPage::onCreateDatabase);
    connect(m_Ui->deleteDbButton, &QPushButton::clicked, this, &ServerUserPage::onDeleteDatabase);
    connect(m_Ui->refreshDbButton, &QPushButton::clicked, this, &ServerUserPage::onRefreshDbComboBox);
    connect(m_Ui->dbComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ServerUserPage::onCurrentDbChanged);
    connect(m_Ui->saveSettingButton, &QPushButton::clicked, this, &ServerUserPage::onSaveSetting);
    connect(m_Ui->searchButton, &QPushButton::clicked, this, &ServerUserPage::onSearchUsers);
    connect(m_Ui->searchLineEdit, &QLineEdit::returnPressed, this, &ServerUserPage::onSearchUsers);
    connect(m_Ui->addUserButton, &QPushButton::clicked, this, &ServerUserPage::onAddUser);
    connect(m_Ui->editUserButton, &QPushButton::clicked, this, &ServerUserPage::onEditUser);
    connect(m_Ui->deleteUserButton, &QPushButton::clicked, this, &ServerUserPage::onDeleteUser);

    onRefreshDbComboBox();
}

ServerUserPage::~ServerUserPage()
{
    delete m_Ui;
}

void ServerUserPage::onRefreshDbComboBox()
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

void ServerUserPage::onCreateDatabase()
{
    const QString fileName = m_DatabaseManager->createDatabase();
    if (fileName.isEmpty())
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             QStringLiteral("新建数据库失败"), 2000, this);
        return;
    }
    onRefreshDbComboBox();
    m_Ui->dbComboBox->setCurrentText(fileName);
    ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("已创建数据库文件 %1").arg(fileName), 2000, this);
}

void ServerUserPage::onDeleteDatabase()
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先选择要删除的数据库文件"), 2000, this);
        return;
    }
    showConfirmDialog(
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除数据库文件 %1 吗？\n该文件中的所有用户数据将不可恢复。").arg(fileName),
        QStringLiteral("删除"),
        [this, fileName]() {
            if (!m_DatabaseManager->deleteDatabase(fileName))
            {
                ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                     QStringLiteral("删除数据库文件失败"), 2000, this);
                return;
            }
            onRefreshDbComboBox();
            ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("数据库文件已删除"), 2000, this);
        });
}

void ServerUserPage::onCurrentDbChanged()
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        m_DatabaseManager->closeDatabase();
        loadSettingToUi();
        refreshUserTable();
        updateControlsEnabled(false);
        m_Ui->dbPathLabel->setText(QStringLiteral("未选择数据库"));
        return;
    }

    if (m_DatabaseManager->currentDatabaseName() != fileName && !m_DatabaseManager->openDatabase(fileName))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             QStringLiteral("打开数据库 %1 失败").arg(fileName), 2000, this);
        m_Ui->dbComboBox->blockSignals(true);
        m_Ui->dbComboBox->removeItem(m_Ui->dbComboBox->currentIndex());
        m_Ui->dbComboBox->blockSignals(false);
        onCurrentDbChanged();
        return;
    }

    loadSettingToUi();
    refreshUserTable();
    updateControlsEnabled(true);
    m_Ui->dbPathLabel->setText(DatabaseManager::dataDirectory() + QLatin1Char('/') + fileName);
}

void ServerUserPage::onSaveSetting()
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先选择数据库文件"), 2000, this);
        return;
    }

    const QString ip = m_Ui->ipLineEdit->text().trimmed();
    const QString port = m_Ui->portLineEdit->text().trimmed();

    if (!isValidIpv4(ip))
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("请输入合法的 IPv4 地址"), 2000, this);
        return;
    }
    bool portOk = false;
    const int portValue = port.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("端口必须是 1-65535 的整数"), 2000, this);
        return;
    }

    if (!m_DatabaseManager->setSetting(kSettingPublicIp, ip)
        || !m_DatabaseManager->setSetting(kSettingPublicPort, port))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             QStringLiteral("保存设置失败"), 2000, this);
        return;
    }
    ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("已保存：客户端登录需填写公网地址 %1:%2").arg(ip, port),
                           2500, this);
}

void ServerUserPage::onSearchUsers()
{
    refreshUserTable();
}

void ServerUserPage::onAddUser()
{
    UserEditDialog dialog(false, this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    QString errorMessage;
    if (!m_DatabaseManager->addUser(dialog.username(), dialog.password(), dialog.remark(),
                                    dialog.isEnabled(), dialog.expireAt(), &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 2500, this);
        return;
    }
    refreshUserTable();
    ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("用户已添加"), 2000, this);
}

void ServerUserPage::onEditUser()
{
    const int id = selectedUserId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先在列表中选择要修改的用户"), 2000, this);
        return;
    }

    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    const DatabaseManager::UserInfo* target = nullptr;
    for (const DatabaseManager::UserInfo& user : users)
    {
        if (user.id == id)
        {
            target = &user;
            break;
        }
    }
    if (!target)
    {
        ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                               QStringLiteral("未找到该用户"), 2000, this);
        return;
    }

    UserEditDialog dialog(true, this);
    dialog.setUsername(target->username);
    dialog.setRemark(target->remark);
    dialog.setIsEnabled(target->isEnabled);
    dialog.setExpireAt(target->expireAt);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QString errorMessage;
    if (!m_DatabaseManager->updateUser(id, dialog.username(), dialog.password(), dialog.remark(),
                                       dialog.isEnabled(), dialog.expireAt(), &errorMessage))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             errorMessage, 2500, this);
        return;
    }
    refreshUserTable();
    ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                           QStringLiteral("用户已更新"), 2000, this);
}

void ServerUserPage::onDeleteUser()
{
    const int id = selectedUserId();
    if (id < 0)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("请先在列表中选择要删除的用户"), 2000, this);
        return;
    }
    showConfirmDialog(
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除该用户吗？"),
        QStringLiteral("删除"),
        [this, id]() {
            if (!m_DatabaseManager->deleteUser(id))
            {
                ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                     QStringLiteral("删除用户失败"), 2000, this);
                return;
            }
            refreshUserTable();
            ElaMessageBar::success(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("用户已删除"), 2000, this);
        });
}

void ServerUserPage::loadSettingToUi()
{
    m_Ui->ipLineEdit->setText(m_DatabaseManager->getSetting(kSettingPublicIp));
    m_Ui->portLineEdit->setText(m_DatabaseManager->getSetting(kSettingPublicPort));
}

void ServerUserPage::refreshUserTable()
{
    m_UserModel->setRowCount(0);
    const QList<DatabaseManager::UserInfo> users =
        m_DatabaseManager->queryUsers(m_Ui->searchLineEdit->text());
    for (const DatabaseManager::UserInfo& user : users)
    {
        const int row = m_UserModel->rowCount();
        m_UserModel->insertRow(row);

        QStandardItem* idItem = new QStandardItem(QString::number(user.id));
        idItem->setData(user.id, Qt::UserRole);
        idItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 0, idItem);

        QStandardItem* usernameItem = new QStandardItem(user.username);
        usernameItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 1, usernameItem);

        QStandardItem* remarkItem = new QStandardItem(user.remark);
        remarkItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 2, remarkItem);

        QStandardItem* statusItem = new QStandardItem(user.isEnabled ? QStringLiteral("启用")
                                                                     : QStringLiteral("禁用"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 3, statusItem);

        QStandardItem* expireItem = new QStandardItem(user.expireAt.isEmpty()
                                                          ? QStringLiteral("永不")
                                                          : user.expireAt);
        expireItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 4, expireItem);

        QStandardItem* timeItem = new QStandardItem(user.createdAt);
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 5, timeItem);
    }
}

void ServerUserPage::updateControlsEnabled(bool isDatabaseOpen)
{
    m_Ui->ipLineEdit->setEnabled(isDatabaseOpen);
    m_Ui->portLineEdit->setEnabled(isDatabaseOpen);
    m_Ui->saveSettingButton->setEnabled(isDatabaseOpen);
    m_Ui->searchLineEdit->setEnabled(isDatabaseOpen);
    m_Ui->searchButton->setEnabled(isDatabaseOpen);
    m_Ui->addUserButton->setEnabled(isDatabaseOpen);
    m_Ui->editUserButton->setEnabled(isDatabaseOpen);
    m_Ui->deleteUserButton->setEnabled(isDatabaseOpen);
}

int ServerUserPage::selectedUserId() const
{
    const QModelIndexList selectedRows = m_Ui->userTableView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty())
    {
        return -1;
    }
    const QModelIndex index = selectedRows.first();
    const QStandardItem* item = m_UserModel->item(index.row(), 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void ServerUserPage::showConfirmDialog(const QString& title, const QString& content,
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

    // 隐藏未使用的空文本按钮（中间按钮）
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
        // 右按钮点击后 ElaContentDialog 会自动执行关闭动画
        if (onConfirm)
        {
            onConfirm();
        }
    });
    dialog->exec();
    dialog->deleteLater();
}
