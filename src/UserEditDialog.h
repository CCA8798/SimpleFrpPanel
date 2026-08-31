#ifndef USEREDITDIALOG_H
#define USEREDITDIALOG_H

#include "ElaDialog.h"

class ElaCalendarPicker;
class ElaCheckBox;
class ElaLineEdit;

// 新增/修改用户对话框（基于 ElaDialog，全部使用 ElaWidgetTools 组件）。
// 修改模式下密码留空表示"不修改密码"；支持设置到期时间（空 = 永不过期）。
class UserEditDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit UserEditDialog(bool isEditMode, QWidget* parent = nullptr);

    QString username() const;
    QString password() const;
    QString remark() const;
    bool isEnabled() const;
    QString expireAt() const; // "yyyy-MM-dd"，空串表示永不过期

    void setUsername(const QString& username);
    void setRemark(const QString& remark);
    void setIsEnabled(bool enabled);
    void setExpireAt(const QString& date);

private:
    bool m_IsEditMode = false;
    ElaLineEdit* m_UsernameEdit = nullptr;
    ElaLineEdit* m_PasswordEdit = nullptr;
    ElaLineEdit* m_RemarkEdit = nullptr;
    ElaCheckBox* m_EnabledCheck = nullptr;
    ElaCheckBox* m_NeverExpireCheck = nullptr;
    ElaCalendarPicker* m_ExpirePicker = nullptr;
};

#endif // USEREDITDIALOG_H
