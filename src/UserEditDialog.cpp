#include "UserEditDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

UserEditDialog::UserEditDialog(bool isEditMode, QWidget* parent)
    : QDialog(parent)
    , m_IsEditMode(isEditMode)
{
    setWindowTitle(isEditMode ? QStringLiteral("修改用户") : QStringLiteral("新增用户"));
    setMinimumWidth(360);

    m_UsernameEdit = new QLineEdit(this);
    m_UsernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_UsernameEdit->setMaxLength(32);

    m_PasswordEdit = new QLineEdit(this);
    m_PasswordEdit->setEchoMode(QLineEdit::Password);
    m_PasswordEdit->setMaxLength(64);
    if (isEditMode)
    {
        m_PasswordEdit->setPlaceholderText(QStringLiteral("留空则不修改密码"));
    }
    else
    {
        m_PasswordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    }

    m_RemarkEdit = new QLineEdit(this);
    m_RemarkEdit->setPlaceholderText(QStringLiteral("备注（可选）"));
    m_RemarkEdit->setMaxLength(128);

    m_EnabledCheck = new QCheckBox(QStringLiteral("启用该账号"), this);
    m_EnabledCheck->setChecked(true);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("用户名："), m_UsernameEdit);
    formLayout->addRow(QStringLiteral("密码："), m_PasswordEdit);
    formLayout->addRow(QStringLiteral("备注："), m_RemarkEdit);
    formLayout->addRow(QString(), m_EnabledCheck);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (username().trimmed().isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("用户名不能为空"));
            return;
        }
        if (!m_IsEditMode && password().isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("密码不能为空"));
            return;
        }
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString UserEditDialog::username() const
{
    return m_UsernameEdit->text();
}

QString UserEditDialog::password() const
{
    return m_PasswordEdit->text();
}

QString UserEditDialog::remark() const
{
    return m_RemarkEdit->text();
}

bool UserEditDialog::isEnabled() const
{
    return m_EnabledCheck->isChecked();
}

void UserEditDialog::setUsername(const QString& username)
{
    m_UsernameEdit->setText(username);
}

void UserEditDialog::setRemark(const QString& remark)
{
    m_RemarkEdit->setText(remark);
}

void UserEditDialog::setIsEnabled(bool enabled)
{
    m_EnabledCheck->setChecked(enabled);
}
