#ifndef TUNNELEDITDIALOG_H
#define TUNNELEDITDIALOG_H

#include "ElaDialog.h"

class ElaCheckBox;
class ElaComboBox;
class ElaLineEdit;

// 新增/修改隧道对话框（基于 ElaDialog，全部使用 ElaWidgetTools 组件）。
// 协议为 http/https 时启用自定义域名输入。
class TunnelEditDialog : public ElaDialog
{
    Q_OBJECT

public:
    explicit TunnelEditDialog(bool isEditMode, QWidget* parent = nullptr);

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

private:
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
