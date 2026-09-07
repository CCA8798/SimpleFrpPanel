#include "HomePage.h"
#include "Information.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QTimer>

#include "ElaText.h"
#include "ElaTheme.h"
#include "OverviewCard.h"
#include "ui_HomePage.h"

namespace {
// 字节数格式化为可读文本（与流量统计页一致）
QString formatBytes(qint64 bytes)
{
    if (bytes < 1024)
    {
        return QStringLiteral("%1 B").arg(bytes);
    }
    double value = static_cast<double>(bytes);
    const QStringList units = {QStringLiteral("KB"), QStringLiteral("MB"),
                               QStringLiteral("GB"), QStringLiteral("TB"),
                               QStringLiteral("PB")};
    int unitIndex = -1;
    while (value >= 1024.0 && unitIndex < units.size() - 1)
    {
        value /= 1024.0;
        ++unitIndex;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 2).arg(units[unitIndex]);
}

// 状态色：运行/已启用绿、停止/未连接灰
const QColor kStatusGreen(0x4C, 0xAF, 0x50);
const QColor kStatusGray(0x9E, 0x9E, 0x9E);

// 状态行：hasDot + 状态色 + 运行文本
OverviewCard::Row statusRow(const QString& key, bool ok, const QString& okText,
                            const QString& failText)
{
    OverviewCard::Row row;
    row.key = key;
    row.hasDot = true;
    row.dotColor = ok ? kStatusGreen : kStatusGray;
    row.value = ok ? okText : failText;
    row.valueColor = ok ? kStatusGreen : kStatusGray;
    return row;
}

// 普通键值行
OverviewCard::Row textRow(const QString& key, const QString& value)
{
    OverviewCard::Row row;
    row.key = key;
    row.value = value;
    return row;
}

// 端口文本："运行中（端口 8966）" / "未运行"（自由函数：统一翻译上下文）
QString runningText(bool running, int port)
{
    return running
               ? QCoreApplication::translate("SimpleFrpPanel", "运行中（端口 %1）").arg(port)
               : QCoreApplication::translate("SimpleFrpPanel", "未运行");
}
} // namespace

HomePage::HomePage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::HomePage())
{
    m_Ui->setupUi(this);

    m_Ui->titleLabel->setText(QStringLiteral("SimpleFrpPanel v%1").arg(g_GlobalInformation.appVersion));

    m_Ui->nowTimeLCDScreen->setIsUseAutoClock(true);
    m_Ui->nowTimeLCDScreen->setStyleSheet(
        QStringLiteral("#ElaLcdScreen { background-color: %1; color: %2; }")
            .arg(ElaThemeColor(eTheme->getThemeMode(), BasicBase).name(),
                 ElaThemeColor(eTheme->getThemeMode(), BasicText).name()));

    m_Ui->overviewTitleLabel->setTextPixelSize(14);

    // 创建四张自绘总览卡：运行状态 / 数据统计 / 流量统计 / 客户端会话
    // 注意：直接 tr()（lupdate 只识别直接的 tr/translate 字面量调用）
    m_StatusCard = new OverviewCard(this);
    m_StatusCard->setTitle(tr("运行状态"));
    m_StatusCard->setCardIcon(ElaIconType::Server);

    m_StatCard = new OverviewCard(this);
    m_StatCard->setTitle(tr("数据统计"));
    m_StatCard->setCardIcon(ElaIconType::UserGroup);

    m_TrafficCard = new OverviewCard(this);
    m_TrafficCard->setTitle(tr("流量统计"));
    m_TrafficCard->setCardIcon(ElaIconType::ArrowTrendUp);

    m_SessionCard = new OverviewCard(this);
    m_SessionCard->setTitle(tr("客户端会话"));
    m_SessionCard->setCardIcon(ElaIconType::Laptop);

    const int cardSpacing = 12;
    m_Ui->overviewGridLayout->setHorizontalSpacing(cardSpacing);
    m_Ui->overviewGridLayout->setVerticalSpacing(cardSpacing);
    m_Ui->overviewGridLayout->addWidget(m_StatusCard, 0, 0);
    m_Ui->overviewGridLayout->addWidget(m_StatCard, 0, 1);
    m_Ui->overviewGridLayout->addWidget(m_TrafficCard, 1, 0);
    m_Ui->overviewGridLayout->addWidget(m_SessionCard, 1, 1);

    // 每秒刷新总览数据（数据源：g_GlobalInformation，各页面轮询写入）
    m_RefreshTimer = new QTimer(this);
    m_RefreshTimer->setInterval(1000);
    connect(m_RefreshTimer, &QTimer::timeout, this, &HomePage::refreshOverview);
    m_RefreshTimer->start();
    refreshOverview();
}

HomePage::~HomePage()
{
    delete m_Ui;
}

void HomePage::refreshOverview()
{
    const GlobalInformation& g = g_GlobalInformation;

    // —— 运行状态卡 ——
    {
        QList<OverviewCard::Row> rows;
        rows.append(statusRow(tr("frps"), g.frpsRunning, runningText(true, g.frpsBindPort),
                              tr("未运行")));
        rows.append(statusRow(tr("面板服务"), g.panelRunning, runningText(true, g.panelPort),
                              tr("未运行")));
        if (g.serverDbOpen)
        {
            rows.append(statusRow(tr("数据库"), true, g.serverDbName, tr("未打开")));
        }
        else
        {
            rows.append(statusRow(tr("数据库"), false, tr("未打开"), tr("未打开")));
        }
        if (g.frpsWebPort > 0)
        {
            rows.append(textRow(tr("仪表盘"),
                                tr("端口 %1（frps webServer）").arg(g.frpsWebPort)));
        }
        else
        {
            rows.append(textRow(tr("仪表盘"), tr("未启用")));
        }
        m_StatusCard->setRows(rows);
    }

    // —— 数据统计卡 ——
    {
        QList<OverviewCard::Row> rows;
        if (g.serverDbOpen)
        {
            rows.append(textRow(tr("用户"), QString::number(g.usersCount)));
            rows.append(textRow(tr("隧道"),
                                tr("%1（启用 %2）")
                                    .arg(g.tunnelsCount)
                                    .arg(g.tunnelsEnabledCount)));
            rows.append(textRow(tr("客户端隧道"),
                                tr("%1（启用 %2）")
                                    .arg(g.clientTunnelsCount)
                                    .arg(g.clientTunnelsEnabledCount)));
        }
        else
        {
            rows.append(textRow(tr("服务端数据库"), tr("未打开")));
        }
        m_StatCard->setRows(rows);
    }

    // —— 流量统计卡 ——
    {
        QList<OverviewCard::Row> rows;
        if (g.serverDbOpen)
        {
            rows.append(textRow(tr("今日接收"), formatBytes(g.todayBytesIn)));
            rows.append(textRow(tr("今日发送"), formatBytes(g.todayBytesOut)));
            rows.append(textRow(tr("累计接收"), formatBytes(g.totalBytesIn)));
            rows.append(textRow(tr("累计发送"), formatBytes(g.totalBytesOut)));
        }
        else
        {
            rows.append(textRow(tr("服务端数据库"), tr("未打开")));
        }
        m_TrafficCard->setRows(rows);
    }

    // —— 客户端会话卡 ——
    {
        QList<OverviewCard::Row> rows;
        if (g.clientLoggedIn)
        {
            rows.append(statusRow(tr("登录状态"), true,
                                  tr("已登录（%1）").arg(g.clientUserName), tr("未登录")));
            rows.append(textRow(tr("面板服务器"),
                                g.clientServerPort > 0
                                    ? QStringLiteral("%1:%2")
                                          .arg(g.clientServerHost)
                                          .arg(g.clientServerPort)
                                    : QStringLiteral("--")));
            if (g.frpcRunning)
            {
                rows.append(statusRow(tr("frpc"), true,
                                      g.frpcServerPort > 0
                                          ? tr("运行中 → %1:%2")
                                                .arg(g.frpcServerHost)
                                                .arg(g.frpcServerPort)
                                          : tr("运行中"),
                                      tr("未运行")));
            }
            else
            {
                rows.append(statusRow(tr("frpc"), false, tr("运行中"), tr("未运行")));
            }
            rows.append(textRow(tr("我的隧道"),
                                tr("%1（启用 %2）")
                                    .arg(g.clientTunnelsCount)
                                    .arg(g.clientTunnelsEnabledCount)));
        }
        else
        {
            rows.append(statusRow(tr("登录状态"), false, tr("已登录"), tr("未登录")));
            rows.append(textRow(tr("面板服务器"),
                                g.clientServerPort > 0
                                    ? QStringLiteral("%1:%2")
                                          .arg(g.clientServerHost)
                                          .arg(g.clientServerPort)
                                    : QStringLiteral("--")));
            rows.append(statusRow(tr("frpc"), false, tr("运行中"), tr("未运行")));
            rows.append(textRow(tr("我的隧道"), QStringLiteral("--")));
        }
        m_SessionCard->setRows(rows);
    }
}
