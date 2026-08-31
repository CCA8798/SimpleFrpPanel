#ifndef USEREDITDIALOG_H
#define USEREDITDIALOG_H

#include <QDialog>

class QCheckBox;
class QLineEdit;

// 新增/修改用户对话框（代码构建）。
// 修改模式下密码留空表示"不修改密码"。
class UserEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserEditDialog(bool isEditMode, QWidget* parent = nullptr);

    QString username() const;
    QString password() const;
    QString remark() const;
    bool isEnabled() const;

    void setUsername(const QString& username);
    void setRemark(const QString& remark);
    void setIsEnabled(bool enabled);

private:
    bool m_IsEditMode = false;
    QLineEdit* m_UsernameEdit = nullptr;
    QLineEdit* m_PasswordEdit = nullptr;
    QLineEdit* m_RemarkEdit = nullptr;
    QCheckBox* m_EnabledCheck = nullptr;
};

#endif // USEREDITDIALOG_H
