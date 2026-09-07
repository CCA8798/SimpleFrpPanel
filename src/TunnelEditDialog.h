#ifndef TUNNELEDITDIALOG_H
#define TUNNELEDITDIALOG_H

#include "ElaDialog.h"

class ElaCheckBox;
class ElaComboBox;
class ElaLineEdit;

// 新增/修改隧道对话框（基于 ElaDialog，全部使用 ElaWidgetTools 组件）。
// 协议为 http/https 时启用自定义域名输入。
// 新增模式带"填写记忆"：服务端与客户端各自记忆一份（draftKey 区分），
// 每次打开自动填充上一次填写的数据（确定/取消都会保存），便于连续添加相似隧道。
class TunnelEditDialog : public ElaDialog
{
    Q_OBJECT

public:
    // isEditMode：true=修改既有隧道（不读取/不保存草稿）
    // draftKey：草稿记忆分组标识，如 "server" / "client"（空 = 共用默认组）
    explicit TunnelEditDialog(bool isEditMode, const QString& draftKey,
                              QWidget* parent = nullptr);

    QString name() const;
    QString protocol() const;
    int remotePort() const;
    QString localIp() const;
    int localPort() const;
    QString customDomain() const;
    bool isEnabled() const;
    QString remark() const;

    void setName(const QString& name);
    void setProtocol(const QString& protocol);
    void setRemotePort(int remotePort);
    void setLocalIp(const QString& localIp);
    void setLocalPort(int localPort);
    void setCustomDomain(const QString& customDomain);
    void setIsEnabled(bool enabled);
    void setRemark(const QString& remark);

protected:
    // 新增模式下：关闭时保存填写快照（取消/确定都会保存），下次打开自动还原
    void accept() override;
    void reject() override;

private:
    void saveDraft() const;
    QString draftSection() const;

    bool m_IsEditMode = false;
    QString m_DraftKey;
    ElaLineEdit* m_NameEdit = nullptr;
    ElaComboBox* m_ProtocolCombo = nullptr;
    ElaLineEdit* m_RemotePortEdit = nullptr;
    ElaLineEdit* m_LocalIpEdit = nullptr;
    ElaLineEdit* m_LocalPortEdit = nullptr;
    ElaLineEdit* m_DomainEdit = nullptr;
    ElaCheckBox* m_EnabledCheck = nullptr;
    ElaLineEdit* m_RemarkEdit = nullptr;
};

#endif // TUNNELEDITDIALOG_H
