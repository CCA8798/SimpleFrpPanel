#ifndef CLIENTTUNNELPAGE_H
#define CLIENTTUNNELPAGE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class FrpsManager;
class PanelClient;
class QStandardItemModel;
class QTimer;

namespace Ui {
class ClientTunnelPage;
}

// 客户端 · 隧道管理页：
// - 顶部：服务器地址/端口/用户名/密码登录（连接服务端面板 API），登录后显示端口配额
// - 中部：我的隧道列表（行内开关启停、增删查改、状态灯显示运行状况）
// - 底部：连接与操作日志
// 具体使用哪个端口由本页在服务端界定的配额范围内自选，服务端统一校验
class ClientTunnelPage : public QWidget
{
    Q_OBJECT

public:
    explicit ClientTunnelPage(QWidget* parent = nullptr);
    ~ClientTunnelPage() override;

protected:
    // 每次切换到本面板时刷新一次
    void showEvent(QShowEvent* event) override;

private slots:
    void onLoginClicked();
    void onLogoutClicked();
    void onRefreshClicked();
    void onAddTunnel();
    void onEditTunnel();
    void onDeleteTunnel();
    void onClearLog();
    void onBrowseFrpc();
    void onToggleFrpc();

private:
    void updateLoginUi();
    void updateQuotaLabel();
    void updateFrpcStatusUi();
    void rebuildFrpcConfigIfRunning();
    QString frpcConfigSignature() const;
    void refreshTunnelTable();
    void appendLog(const QString& text);
    int selectedTunnelId() const;

    Ui::ClientTunnelPage* m_Ui = nullptr;
    PanelClient* m_Client = nullptr;
    FrpsManager* m_FrpcManager = nullptr;
    QStandardItemModel* m_TunnelModel = nullptr;
    QJsonArray m_Tunnels;
    QJsonObject m_Quota;
    QJsonObject m_ServerInfo;
    bool m_FrpsRunning = false;
    bool m_PendingLogin = false;
    QTimer* m_PollTimer = nullptr;
    QString m_LastTunnelsSignature;
    QString m_LastFrpcConfigSignature; // frpc 配置签名（未变化不重启 frpc）
};

#endif // CLIENTTUNNELPAGE_H
