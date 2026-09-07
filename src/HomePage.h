#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>

class QTimer;
class OverviewCard;

namespace Ui {
class HomePage;
}

// 中央页面：普通 QWidget，界面绑定 src/HomePage.ui。
// 在 Qt Designer 中编辑 HomePage.ui，通过"提升法"将控件提升为 Ela 组件
// （如 ElaPushButton，头文件 ElaPushButton.h），编译时即可链接真实类。
// 下半部为"运行总览"：2x2 自绘卡片（OverviewCard），每 1 秒读取
// g_GlobalInformation（由服务端/客户端页面各自写入）刷新内容，
// 卡片随 Ela 深浅主题自动换肤。
class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget* parent = nullptr);
    ~HomePage() override;

private:
    // 按 g_GlobalInformation 刷新四张总览卡
    void refreshOverview();

    Ui::HomePage* m_Ui = nullptr;
    QTimer* m_RefreshTimer = nullptr;
    OverviewCard* m_StatusCard = nullptr;   // 运行状态
    OverviewCard* m_StatCard = nullptr;     // 数据统计
    OverviewCard* m_TrafficCard = nullptr;  // 流量统计
    OverviewCard* m_SessionCard = nullptr;  // 客户端会话
};

#endif // HOMEPAGE_H
