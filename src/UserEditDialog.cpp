#include "UserEditDialog.h"

#include <QDate>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaCalendarPicker.h"
#include "ElaCheckBox.h"
#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

UserEditDialog::UserEditDialog(bool isEditMode, QWidget* parent)
    : ElaDialog(parent)
    , m_IsEditMode(isEditMode)
{
    setWindowTitle(isEditMode ? QStringLiteral("修改用户") : QStringLiteral("新增用户"));
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setIsFixedSize(true);
    setMinimumWidth(440);

    m_UsernameEdit = new ElaLineEdit(this);
    m_UsernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_UsernameEdit->setMaxLength(32);

    m_PasswordEdit = new ElaLineEdit(this);
    m_PasswordEdit->setEchoMode(QLineEdit::Password);
    m_PasswordEdit->setMaxLength(64);
    m_PasswordEdit->setPlaceholderText(isEditMode ? QStringLiteral("留空则不修改密码")
                                                  : QStringLiteral("请输入密码"));

    m_RemarkEdit = new ElaLineEdit(this);
    m_RemarkEdit->setPlaceholderText(QStringLiteral("备注（可选）"));
    m_RemarkEdit->setMaxLength(128);

    // 到期时间：默认永不过期；取消勾选后可用日历选择器指定日期
    m_NeverExpireCheck = new ElaCheckBox(QStringLiteral("永不过期"), this);
    m_NeverExpireCheck->setChecked(true);
    m_ExpirePicker = new ElaCalendarPicker(this);
    m_ExpirePicker->setSelectedDate(QDate::currentDate().addYears(1));
    m_ExpirePicker->setEnabled(false);

    m_EnabledCheck = new ElaCheckBox(QStringLiteral("启用该账号"), this);
    m_EnabledCheck->setChecked(true);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow(new ElaText(QStringLiteral("用户名："), this), m_UsernameEdit);
    formLayout->addRow(new ElaText(QStringLiteral("密码："), this), m_PasswordEdit);
    formLayout->addRow(new ElaText(QStringLiteral("备注："), this), m_RemarkEdit);
    formLayout->addRow(new ElaText(QStringLiteral("到期时间："), this), m_ExpirePicker);
    formLayout->addRow(QString(), m_NeverExpireCheck);
    formLayout->addRow(QString(), m_EnabledCheck);

    ElaPushButton* cancelButton = new ElaPushButton(QStringLiteral("取消"), this);
    ElaPushButton* okButton = new ElaPushButton(QStringLiteral("确定"), this);
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(8);
    mainLayout->addLayout(buttonLayout);

    connect(m_NeverExpireCheck, &ElaCheckBox::toggled, m_ExpirePicker, &ElaCalendarPicker::setEnabled);
    connect(cancelButton, &ElaPushButton::clicked, this, &QDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, [this]() {
        if (username().trimmed().isEmpty())
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("用户名不能为空"), 2000, this);
            return;
        }
        if (!m_IsEditMode && password().isEmpty())
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("密码不能为空"), 2000, this);
            return;
        }
        accept();
    });
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

QString UserEditDialog::expireAt() const
{
    if (m_NeverExpireCheck->isChecked())
    {
        return QString();
    }
    return m_ExpirePicker->getSelectedDate().toString(QStringLiteral("yyyy-MM-dd"));
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

void UserEditDialog::setExpireAt(const QString& date)
{
    if (date.trimmed().isEmpty())
    {
        m_NeverExpireCheck->setChecked(true);
        return;
    }
    const QDate parsedDate = QDate::fromString(date.trimmed(), QStringLiteral("yyyy-MM-dd"));
    if (parsedDate.isValid())
    {
        m_ExpirePicker->setSelectedDate(parsedDate);
        m_NeverExpireCheck->setChecked(false);
    }
}
