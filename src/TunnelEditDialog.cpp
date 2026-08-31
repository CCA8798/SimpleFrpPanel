#include "TunnelEditDialog.h"

#include <QCoreApplication>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QSettings>
#include <QVBoxLayout>

#include "ElaCheckBox.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

namespace {
const QStringList kProtocols = QStringList()
                               << QStringLiteral("tcp") << QStringLiteral("udp")
                               << QStringLiteral("http") << QStringLiteral("https");

// 新增隧道草稿快照（config.ini 的 tunnel_draft 分组）
const QString kDraftSection = QStringLiteral("tunnel_draft");

QSettings draftSettings()
{
    return QSettings(QCoreApplication::applicationDirPath() + QStringLiteral("/config.ini"),
                     QSettings::IniFormat);
}

ElaText* makeCompactLabel(const QString& text, QWidget* parent)
{
    ElaText* label = new ElaText(text, parent);
    label->setTextPixelSize(12);
    return label;
}
} // namespace

TunnelEditDialog::TunnelEditDialog(bool isEditMode, QWidget* parent)
    : ElaDialog(parent)
    , m_IsEditMode(isEditMode)
{
    setWindowTitle(isEditMode ? QStringLiteral("修改隧道") : QStringLiteral("新增隧道"));
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    setIsFixedSize(true);

    // 整体调小字体（子控件继承）
    QFont compactFont = font();
    compactFont.setPointSize(9);
    setFont(compactFont);

    auto makeEdit = [this](const QString& placeholder, int maxLength) {
        ElaLineEdit* edit = new ElaLineEdit(this);
        edit->setFixedHeight(32);
        edit->setPlaceholderText(placeholder);
        edit->setMaxLength(maxLength);
        return edit;
    };

    m_NameEdit = makeEdit(QStringLiteral("请输入隧道名称"), 64);

    m_ProtocolCombo = new ElaComboBox(this);
    m_ProtocolCombo->setFixedHeight(32);
    m_ProtocolCombo->addItems(kProtocols);

    m_RemotePortEdit = makeEdit(QStringLiteral("服务端监听端口 (1-65535)"), 5);
    m_RemotePortEdit->setValidator(new QIntValidator(1, 65535, this));

    m_LocalIpEdit = makeEdit(QStringLiteral("目标内网 IP，如 192.168.1.10"), 46);

    m_LocalPortEdit = makeEdit(QStringLiteral("目标端口 (1-65535)"), 5);
    m_LocalPortEdit->setValidator(new QIntValidator(1, 65535, this));

    m_DomainEdit = makeEdit(QStringLiteral("自定义域名（http/https 必填）"), 128);

    m_EnabledCheck = new ElaCheckBox(QStringLiteral("启用该隧道"), this);
    m_EnabledCheck->setChecked(true);

    m_RemarkEdit = makeEdit(QStringLiteral("备注（可选）"), 128);

    QFormLayout* formLayout = new QFormLayout;
    formLayout->setVerticalSpacing(6);
    formLayout->setHorizontalSpacing(12);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->addRow(makeCompactLabel(QStringLiteral("隧道名称："), this), m_NameEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("协议："), this), m_ProtocolCombo);
    formLayout->addRow(makeCompactLabel(QStringLiteral("远端端口："), this), m_RemotePortEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("目标 IP："), this), m_LocalIpEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("目标端口："), this), m_LocalPortEdit);
    formLayout->addRow(makeCompactLabel(QStringLiteral("自定义域名："), this), m_DomainEdit);
    formLayout->addRow(QString(), m_EnabledCheck);
    formLayout->addRow(makeCompactLabel(QStringLiteral("备注："), this), m_RemarkEdit);

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

    // 协议联动：http/https 不需要远端端口（走域名），仅需自定义域名
    const auto updateProtocolState = [this]() {
        const QString protocol = m_ProtocolCombo->currentText();
        const bool isHttpLike = (protocol == QStringLiteral("http")
                                 || protocol == QStringLiteral("https"));
        m_RemotePortEdit->setEnabled(!isHttpLike);
        m_RemotePortEdit->setPlaceholderText(isHttpLike
                                                 ? QStringLiteral("http/https 走域名，无需端口")
                                                 : QStringLiteral("服务端监听端口 (1-65535)"));
        m_DomainEdit->setEnabled(isHttpLike);
    };
    connect(m_ProtocolCombo, QOverload<int>::of(&ElaComboBox::currentIndexChanged),
            this, updateProtocolState);
    updateProtocolState();

    connect(cancelButton, &ElaPushButton::clicked, this, &QDialog::reject);
    connect(okButton, &ElaPushButton::clicked, this, [this]() {
        if (name().trimmed().isEmpty())
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("隧道名称不能为空"), 2000, this);
            return;
        }
        const QString protocol = this->protocol();
        const bool isHttpLike = (protocol == QStringLiteral("http")
                                 || protocol == QStringLiteral("https"));
        if (!isHttpLike && (remotePort() < 1 || remotePort() > 65535))
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("远端端口必须是 1-65535 的整数"), 2000, this);
            return;
        }
        if (localPort() < 1 || localPort() > 65535)
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("目标端口必须是 1-65535 的整数"), 2000, this);
            return;
        }
        if (localIp().trimmed().isEmpty())
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("目标内网 IP 不能为空"), 2000, this);
            return;
        }
        if (isHttpLike && customDomain().trimmed().isEmpty())
        {
            ElaMessageBar::warning(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("http/https 隧道必须填写自定义域名"), 2000, this);
            return;
        }
        accept();
    });

    // 新增模式下：还原上次填写的内容（快照）
    bool draftRestored = false;
    if (!isEditMode)
    {
        QSettings settings = draftSettings();
        settings.beginGroup(kDraftSection);
        if (settings.contains(QStringLiteral("name")))
        {
            setName(settings.value(QStringLiteral("name")).toString());
            setProtocol(settings.value(QStringLiteral("protocol"), QStringLiteral("tcp")).toString());
            setRemotePort(settings.value(QStringLiteral("remotePort"), 0).toInt());
            setLocalIp(settings.value(QStringLiteral("localIp")).toString());
            setLocalPort(settings.value(QStringLiteral("localPort"), 0).toInt());
            setCustomDomain(settings.value(QStringLiteral("customDomain")).toString());
            setIsEnabled(settings.value(QStringLiteral("enabled"), true).toBool());
            setRemark(settings.value(QStringLiteral("remark")).toString());
            draftRestored = true;
        }
        settings.endGroup();
    }

    // 按紧凑内容锁定窗口尺寸
    adjustSize();
    setFixedSize(qMax(size().width(), 400), size().height());

    if (draftRestored)
    {
        ElaMessageBar::information(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                                   QStringLiteral("已还原上次未保存的内容"), 2000, this);
    }
}

QString TunnelEditDialog::name() const
{
    return m_NameEdit->text();
}

QString TunnelEditDialog::protocol() const
{
    return m_ProtocolCombo->currentText();
}

int TunnelEditDialog::remotePort() const
{
    return m_RemotePortEdit->text().toInt();
}

QString TunnelEditDialog::localIp() const
{
    return m_LocalIpEdit->text();
}

int TunnelEditDialog::localPort() const
{
    return m_LocalPortEdit->text().toInt();
}

QString TunnelEditDialog::customDomain() const
{
    return m_DomainEdit->text();
}

bool TunnelEditDialog::isEnabled() const
{
    return m_EnabledCheck->isChecked();
}

QString TunnelEditDialog::remark() const
{
    return m_RemarkEdit->text();
}

void TunnelEditDialog::setName(const QString& name)
{
    m_NameEdit->setText(name);
}

void TunnelEditDialog::setProtocol(const QString& protocol)
{
    const int index = m_ProtocolCombo->findText(protocol);
    m_ProtocolCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void TunnelEditDialog::setRemotePort(int remotePort)
{
    m_RemotePortEdit->setText(QString::number(remotePort));
}

void TunnelEditDialog::setLocalIp(const QString& localIp)
{
    m_LocalIpEdit->setText(localIp);
}

void TunnelEditDialog::setLocalPort(int localPort)
{
    m_LocalPortEdit->setText(QString::number(localPort));
}

void TunnelEditDialog::setCustomDomain(const QString& customDomain)
{
    m_DomainEdit->setText(customDomain);
}

void TunnelEditDialog::setIsEnabled(bool enabled)
{
    m_EnabledCheck->setChecked(enabled);
}

void TunnelEditDialog::setRemark(const QString& remark)
{
    m_RemarkEdit->setText(remark);
}

void TunnelEditDialog::accept()
{
    // 新增模式下保存快照：确定成功也保存，方便连续添加相似隧道
    if (!m_IsEditMode)
    {
        saveDraft();
    }
    ElaDialog::accept();
}

void TunnelEditDialog::reject()
{
    // 新增模式下保存快照：取消（含点右上角关闭）后，下次打开可还原
    if (!m_IsEditMode)
    {
        saveDraft();
    }
    ElaDialog::reject();
}

void TunnelEditDialog::saveDraft() const
{
    // 完全空白的填写不保存（避免下次打开提示"已还原"却什么都没有）
    if (name().trimmed().isEmpty() && localIp().trimmed().isEmpty()
        && customDomain().trimmed().isEmpty() && remark().trimmed().isEmpty()
        && remotePort() == 0 && localPort() == 0)
    {
        return;
    }
    QSettings settings = draftSettings();
    settings.beginGroup(kDraftSection);
    settings.setValue(QStringLiteral("name"), name());
    settings.setValue(QStringLiteral("protocol"), protocol());
    settings.setValue(QStringLiteral("remotePort"), remotePort());
    settings.setValue(QStringLiteral("localIp"), localIp());
    settings.setValue(QStringLiteral("localPort"), localPort());
    settings.setValue(QStringLiteral("customDomain"), customDomain());
    settings.setValue(QStringLiteral("enabled"), isEnabled());
    settings.setValue(QStringLiteral("remark"), remark());
    settings.endGroup();
}
