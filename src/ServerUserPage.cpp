#include "ServerUserPage.h"

#include <QHeaderView>
#include <QIntValidator>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardItemModel>

#include "DatabaseManager.h"
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

    // 布局占比：数据库区 30% / 用户区 70%
    m_Ui->mainLayout->setStretch(0, 3);
    m_Ui->mainLayout->setStretch(1, 7);

    // 端口输入校验
    m_Ui->portLineEdit->setValidator(new QIntValidator(1, 65535, this));

    // 用户表模型：ID / 用户名 / 备注 / 状态 / 创建时间
    m_UserModel->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("ID") << QStringLiteral("用户名")
                      << QStringLiteral("备注") << QStringLiteral("状态")
                      << QStringLiteral("创建时间"));
    m_Ui->userTableView->setModel(m_UserModel);
    m_Ui->userTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_Ui->userTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_Ui->userTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_Ui->userTableView->verticalHeader()->setVisible(false);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_Ui->userTableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

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
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("新建数据库失败"));
        return;
    }
    onRefreshDbComboBox();
    m_Ui->dbComboBox->setCurrentText(fileName);
}

void ServerUserPage::onDeleteDatabase()
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的数据库文件"));
        return;
    }
    const QMessageBox::StandardButton result = QMessageBox::question(
        this, QStringLiteral("确认删除"),
        QStringLiteral("确定要删除数据库文件 %1 吗？\n该文件中的所有用户数据将不可恢复。").arg(fileName));
    if (result != QMessageBox::Yes)
    {
        return;
    }
    if (!m_DatabaseManager->deleteDatabase(fileName))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("删除数据库文件失败"));
        return;
    }
    onRefreshDbComboBox();
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
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("打开数据库 %1 失败").arg(fileName));
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
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择数据库文件"));
        return;
    }

    const QString ip = m_Ui->ipLineEdit->text().trimmed();
    const QString port = m_Ui->portLineEdit->text().trimmed();

    if (!isValidIpv4(ip))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入合法的 IPv4 地址"));
        return;
    }
    bool portOk = false;
    const int portValue = port.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535)
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("端口必须是 1-65535 的整数"));
        return;
    }

    if (!m_DatabaseManager->setSetting(kSettingPublicIp, ip)
        || !m_DatabaseManager->setSetting(kSettingPublicPort, port))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("保存设置失败"));
        return;
    }
    QMessageBox::information(
        this, QStringLiteral("提示"),
        QStringLiteral("已保存：客户端登录需填写公网地址 %1:%2").arg(ip, port));
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
                                    dialog.isEnabled(), &errorMessage))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), errorMessage);
        return;
    }
    refreshUserTable();
}

void ServerUserPage::onEditUser()
{
    const int id = selectedUserId();
    if (id < 0)
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在列表中选择要修改的用户"));
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
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未找到该用户"));
        return;
    }

    UserEditDialog dialog(true, this);
    dialog.setUsername(target->username);
    dialog.setRemark(target->remark);
    dialog.setIsEnabled(target->isEnabled);
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QString errorMessage;
    if (!m_DatabaseManager->updateUser(id, dialog.username(), dialog.password(), dialog.remark(),
                                       dialog.isEnabled(), &errorMessage))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), errorMessage);
        return;
    }
    refreshUserTable();
}

void ServerUserPage::onDeleteUser()
{
    const int id = selectedUserId();
    if (id < 0)
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在列表中选择要删除的用户"));
        return;
    }
    const QMessageBox::StandardButton result = QMessageBox::question(
        this, QStringLiteral("确认删除"), QStringLiteral("确定要删除该用户吗？"));
    if (result != QMessageBox::Yes)
    {
        return;
    }
    if (!m_DatabaseManager->deleteUser(id))
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("删除用户失败"));
        return;
    }
    refreshUserTable();
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

        m_UserModel->setItem(row, 1, new QStandardItem(user.username));
        m_UserModel->setItem(row, 2, new QStandardItem(user.remark));

        QStandardItem* statusItem = new QStandardItem(user.isEnabled ? QStringLiteral("启用")
                                                                     : QStringLiteral("禁用"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 3, statusItem);

        QStandardItem* timeItem = new QStandardItem(user.createdAt);
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_UserModel->setItem(row, 4, timeItem);
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
