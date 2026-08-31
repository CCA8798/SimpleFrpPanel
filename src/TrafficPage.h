#ifndef TRAFFICPAGE_H
#define TRAFFICPAGE_H

#include <QWidget>

class DatabaseManager;
class QShowEvent;
class QStandardItemModel;
class QTimer;

namespace Ui {
class TrafficPage;
}

// 服务端 · 流量统计页：
// - 按 数据库/用户/隧道（含已删除）与日期区间查询流量（接收/发送/合计）
// - "全部时间" = 历史总流量；数据由 TrafficMonitor 从 frps 仪表盘 API 采样入库
class TrafficPage : public QWidget
{
    Q_OBJECT

public:
    explicit TrafficPage(QWidget* parent = nullptr);
    ~TrafficPage() override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onRefreshDbComboBox();
    void onCurrentDbChanged();
    void onCurrentUserChanged();
    void onQueryClicked();
    void onPollRefresh();

private:
    void refreshUserComboBox();
    void refreshTunnelComboBox();
    void runQuery();

    Ui::TrafficPage* m_Ui = nullptr;
    DatabaseManager* m_DatabaseManager = nullptr;
    QStandardItemModel* m_TrafficModel = nullptr;
    QTimer* m_PollTimer = nullptr;
    int m_CurrentUserId = -1; // -1 = 全部用户
    QString m_LastQuerySignature; // 上次查询结果签名（未变化则跳过表格重建）
};

#endif // TRAFFICPAGE_H
