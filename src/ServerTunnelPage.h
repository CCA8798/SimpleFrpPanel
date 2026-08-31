#ifndef SERVERTUNNELPAGE_H
#define SERVERTUNNELPAGE_H

#include <QWidget>

#include <functional>

class DatabaseManager;
class FrpsManager;
class PanelApiServer;
class QStandardItemModel;

namespace Ui {
class ServerTunnelPage;
}

// 服务端 · 隧道管理页：
// - 顶部：数据库 / 用户下拉联动 + frps 进程控制（程序路径 / 绑定端口 / Token / 启停 / 状态）
// - 中部：所选用户的隧道表格（行内开关、增删查改、运行状况）
// - 底部：frps 运行日志面板
class ServerTunnelPage : public QWidget
{
    Q_OBJECT

public:
    explicit ServerTunnelPage(QWidget* parent = nullptr);
    ~ServerTunnelPage() override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onRefreshDbComboBox();
    void onCurrentDbChanged();
    void onRefreshUserComboBox();
    void onCurrentUserChanged();
    void onSearchTunnels();
    void onAddTunnel();
    void onEditTunnel();
    void onDeleteTunnel();
    void onBrowseFrps();
    void onToggleFrps();
    void onClearLog();
    void onSaveQuota();
    void onTogglePanelService();

private:
    void refreshTunnelTable();
    void updateTunnelStatusColumn();
    void updateControlsEnabled();
    void loadQuotaToUi();
    void updatePanelServiceUi();
    void syncPanelServiceWithDb();
    int selectedTunnelId() const;
    void appendLog(const QString& text);
    void updateFrpsStatusUi();
    void applyFrpsConfig(bool restartIfRunning);
    QString frpsConfigPath() const;
    QList<QPair<quint16, quint16>> collectPortRanges() const;
    void showConfirmDialog(const QString& title, const QString& content,
                           const QString& confirmText, std::function<void()> onConfirm);

    Ui::ServerTunnelPage* m_Ui = nullptr;
    DatabaseManager* m_DatabaseManager = nullptr;
    FrpsManager* m_FrpsManager = nullptr;
    PanelApiServer* m_PanelApiServer = nullptr;
    QStandardItemModel* m_TunnelModel = nullptr;
    int m_CurrentUserId = -1;
};

#endif // SERVERTUNNELPAGE_H
