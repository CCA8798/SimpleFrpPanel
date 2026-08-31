#include "UserEditDialog.h"

#include <QDate>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaCalendarPicker.h"
#include "ElaCheckBox.h"
#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

namespace {
ElaText* makeCompactLabel(const QString& text, QWidget* parent)
{
    ElaText* label = new ElaText(text, parent);
    label->setTextPixelSize(12);
    return label;
}
} // namespace

UserEditDialog::UserEditDialog(bool isEditMode, QWidget* parent)
    : ElaDialog(parent)
    , m_IsEditMode(isEditMode)
{
    setWindowTitle(isEditMode ? QStringLiteral("修改用户") : QStringLiteral("新增用户"));
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setIsFixedSize(true);

    // 整体调小字体（子控件继承）
    QFont compactFont = font();
    compactFont.setPointSize(9);
    setFont(compactFont);

    m_UsernameEdit = new ElaLineEdit(this);
    m_UsernameEdit->setFixedHeight(32);
    m_UsernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_UsernameEdit->setMaxLength(32);

    m_PasswordEdit = new ElaLineEdit(this);
    m_PasswordEdit->setFixedHeight(32);
    m_PasswordEdit->setEchoMode(QLineEdit::Password);
    m_PasswordEdit->setMaxLength(64);
    m_PasswordEdit->setPlaceholderText(isEditMode ? QStringLiteral("留空则不修改密码")
                                                  : QStringLiteral("请输入密码"));

    m_RemarkEdit = new ElaLineEdit(this);
    m_RemarkEdit->setFixedHeight(32);
    m_RemarkEdit->setPlaceholderText(QStringLiteral("备注（可选）"));
    m_RemarkEdit->setMaxLength(128);

    // 到期时间：默认永不过期；取消勾选后启用日期选择器
    // 注意：toggled 的参数是"勾选状态"，启用/禁用逻辑取反（勾选=禁用选择器）
    m_NeverExpireCheck = new ElaCheckBox(QStringLiteral("永不过期"), this);
    m_NeverExpireCheck->setChecked(true);
    m_ExpirePicker = new ElaCalendarPicker(this);
    m_ExpirePicker->setFixedHeight(32);
    m_ExpirePicker->setSelectedDate(QDate::currentDate().addYears(1));
    m_ExpirePicker->setEnabled(false);

    m_EnabledCheck = new ElaCheckBox(QStringLiteral("启用该账号"), this);
    m_EnabledCheck->setChecked(true);

    // 到期时间行：日期选择器与"永不过期"内联，减少行数
    QHBoxLayout* expireRow = new QHBoxLayout;
    expireRow->setSpacing(10);
    expireRow->addWidget(m_ExpirePicker);
    expireRow->addWidget(m_NeverExpireCheck);
    expireRow->addStretch();

    QFormLayout* formLayout = new QFormLayout;
    formLayout->setVerticalSpacing(6);
    formLayout->setHorizontalSpacing(12);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->addRow(makeCompactLabel(QStringLiteral("用户名："), this), m_UsernameEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("密码："), this), m_PasswordEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("备注："), this), m_RemarkEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("到期时间："), this), expireRow);
    formLayout->addRow(QString(), m_EnabledCheck);

    ElaPushButton* cancelButton = new ElaPushButton(QStringLiteral("取消"), this);
    ElaPushButton* okButton = new ElaPushButton(QStringLiteral("确定"), this);
    cancelButton->setFixedHeight(32);
    okButton->setFixedHeight(32);
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(okButton);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 10, 20, 12);
    mainLayout->setSpacing(10);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);

    connect(m_NeverExpireCheck, &ElaCheckBox::toggled, this, [this](bool checked) {
        m_ExpirePicker->setEnabled(!checked);
    });
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

    // 按紧凑内容锁定窗口尺寸（宽度不小于 380）
    adjustSize();
    setFixedSize(qMax(size().width(), 380), size().height());
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
