#include "TrafficPage.h"

#include <QApplication>
#include <QDate>
#include <QHeaderView>
#include <QSet>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>

#include "DatabaseManager.h"
#include "ElaCheckBox.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ui_TrafficPage.h"

namespace {
const int kAllUsersId = -1;

// 字节数格式化为可读文本
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

qint64 summaryTotal(const QList<DatabaseManager::TrafficSummary>& summaries)
{
    qint64 total = 0;
    for (const DatabaseManager::TrafficSummary& summary : summaries)
    {
        total += summary.bytesIn + summary.bytesOut;
    }
    return total;
}
} // namespace

TrafficPage::TrafficPage(QWidget* parent)
    : QWidget(parent)
    , m_Ui(new Ui::TrafficPage())
    , m_DatabaseManager(new DatabaseManager(this))
    , m_TrafficModel(new QStandardItemModel(this))
{
    m_Ui->setupUi(this);

    // 顶部固定紧凑
    m_Ui->topFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    const QList<QWidget*> topWidgets = {
        m_Ui->dbComboBox, m_Ui->userComboBox, m_Ui->tunnelComboBox,
        m_Ui->queryButton, m_Ui->refreshButton,
        m_Ui->startDatePicker, m_Ui->endDatePicker,
    };
    for (QWidget* widget : topWidgets)
    {
        widget->setFixedHeight(32);
    }
    m_Ui->totalLabel->setTextPixelSize(12);

    // 日期默认：今天
    m_Ui->startDatePicker->setSelectedDate(QDate::currentDate());
    m_Ui->endDatePicker->setSelectedDate(QDate::currentDate());
    // 全部时间默认勾选（禁用日期选择器）
    connect(m_Ui->allTimeCheck, &ElaCheckBox::toggled, this, [this](bool checked) {
        m_Ui->startDatePicker->setEnabled(!checked);
        m_Ui->endDatePicker->setEnabled(!checked);
    });
    m_Ui->allTimeCheck->setChecked(true);

    // 结果表：用户 / 隧道 / 接收流量 / 发送流量 / 合计
    m_TrafficModel->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("用户") << QStringLiteral("隧道")
                      << QStringLiteral("接收流量") << QStringLiteral("发送流量")
                      << QStringLiteral("合计"));
    m_Ui->trafficTableView->setModel(m_TrafficModel);
    m_Ui->trafficTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_Ui->trafficTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_Ui->trafficTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_Ui->trafficTableView->verticalHeader()->setVisible(false);
    m_Ui->trafficTableView->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_Ui->trafficTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_Ui->trafficTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_Ui->trafficTableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_Ui->trafficTableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_Ui->trafficTableView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    connect(m_Ui->dbComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrafficPage::onCurrentDbChanged);
    connect(m_Ui->userComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrafficPage::onCurrentUserChanged);
    connect(m_Ui->queryButton, &QPushButton::clicked, this, &TrafficPage::onQueryClicked);
    connect(m_Ui->refreshButton, &QPushButton::clicked, this, &TrafficPage::onQueryClicked);

    // 轮询刷新：5 秒一次，流量记录持续更新时表格自动同步
    m_PollTimer = new QTimer(this);
    m_PollTimer->setInterval(5000);
    connect(m_PollTimer, &QTimer::timeout, this, &TrafficPage::onPollRefresh);
    m_PollTimer->start();

    onRefreshDbComboBox();
}

TrafficPage::~TrafficPage()
{
    delete m_Ui;
}

void TrafficPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    onRefreshDbComboBox();
}

void TrafficPage::onRefreshDbComboBox()
{
    const QString previousName = m_Ui->dbComboBox->currentText();
    m_Ui->dbComboBox->blockSignals(true);
    m_Ui->dbComboBox->clear();
    m_Ui->dbComboBox->addItems(m_DatabaseManager->databaseFileNames());
    const int index = m_Ui->dbComboBox->findText(previousName);
    m_Ui->dbComboBox->setCurrentIndex(index >= 0 ? index : 0);
    m_Ui->dbComboBox->blockSignals(false);
    onCurrentDbChanged();
}

void TrafficPage::onCurrentDbChanged()
{
    const QString fileName = m_Ui->dbComboBox->currentText();
    if (fileName.isEmpty())
    {
        m_DatabaseManager->closeDatabase();
        m_CurrentUserId = kAllUsersId;
        m_Ui->userComboBox->clear();
        m_Ui->tunnelComboBox->clear();
        m_LastQuerySignature.clear();
        m_TrafficModel->setRowCount(0);
        m_Ui->totalLabel->setText(QStringLiteral("总流量：-"));
        return;
    }
    if (m_DatabaseManager->currentDatabaseName() != fileName
        && !m_DatabaseManager->openDatabase(fileName))
    {
        ElaMessageBar::error(ElaMessageBarType::TopRight, QStringLiteral("提示"),
                             QStringLiteral("打开数据库 %1 失败").arg(fileName), 2000, this);
        m_Ui->dbComboBox->blockSignals(true);
        m_Ui->dbComboBox->removeItem(m_Ui->dbComboBox->currentIndex());
        m_Ui->dbComboBox->blockSignals(false);
        onCurrentDbChanged();
        return;
    }
    refreshUserComboBox();
    // 延迟到弹层关闭后再查询，避免在弹层事件循环内重建表格造成卡顿
    QTimer::singleShot(0, this, [this]() {
        m_LastQuerySignature.clear();
        runQuery();
    });
}

void TrafficPage::onCurrentUserChanged()
{
    m_CurrentUserId = (m_Ui->userComboBox->currentIndex() >= 0)
                          ? m_Ui->userComboBox->currentData().toInt()
                          : kAllUsersId;
    // 延迟到用户下拉弹层关闭后再刷新隧道下拉与查询
    QTimer::singleShot(0, this, [this]() {
        refreshTunnelComboBox();
        m_LastQuerySignature.clear();
        runQuery();
    });
}

void TrafficPage::refreshUserComboBox()
{
    const int previousUserId = m_Ui->userComboBox->currentData().toInt();
    m_Ui->userComboBox->blockSignals(true);
    m_Ui->userComboBox->clear();
    m_Ui->userComboBox->addItem(QStringLiteral("全部用户"), kAllUsersId);
    const QList<DatabaseManager::UserInfo> users = m_DatabaseManager->queryUsers();
    for (const DatabaseManager::UserInfo& user : users)
    {
        m_Ui->userComboBox->addItem(user.username, user.id);
    }
    const int index = m_Ui->userComboBox->findData(previousUserId);
    m_Ui->userComboBox->setCurrentIndex(index >= 0 ? index : 0);
    m_Ui->userComboBox->blockSignals(false);
    m_CurrentUserId = m_Ui->userComboBox->currentData().toInt();
    refreshTunnelComboBox();
}

void TrafficPage::refreshTunnelComboBox()
{
    const QString previousName = m_Ui->tunnelComboBox->currentText();
    m_Ui->tunnelComboBox->blockSignals(true);
    m_Ui->tunnelComboBox->clear();
    m_Ui->tunnelComboBox->addItem(QStringLiteral("全部隧道"));

    if (m_CurrentUserId > 0)
    {
        // 当前隧道
        QSet<QString> currentNames;
        const QList<DatabaseManager::TunnelInfo> tunnels =
            m_DatabaseManager->queryTunnels(m_CurrentUserId);
        for (const DatabaseManager::TunnelInfo& tunnel : tunnels)
        {
            currentNames.insert(tunnel.name);
            m_Ui->tunnelComboBox->addItem(tunnel.name);
        }
        // 已删除隧道的历史名称（带标记）
        const QStringList historicalNames = m_DatabaseManager->queryTrafficTunnelNames(m_CurrentUserId);
        for (const QString& name : historicalNames)
        {
            if (!currentNames.contains(name))
            {
                m_Ui->tunnelComboBox->addItem(name + QStringLiteral("（已删除）"));
            }
        }
    }
    m_Ui->tunnelComboBox->setEnabled(m_CurrentUserId > 0);
    const int index = m_Ui->tunnelComboBox->findText(previousName);
    m_Ui->tunnelComboBox->setCurrentIndex(index >= 0 ? index : 0);
    m_Ui->tunnelComboBox->blockSignals(false);
}

void TrafficPage::onQueryClicked()
{
    runQuery();
}

void TrafficPage::onPollRefresh()
{
    // 数据库打开时自动重新查询（流量记录持续入库，表格保持最新）
    if (!m_DatabaseManager->isOpen())
    {
        return;
    }
    // 下拉框弹出期间跳过自动刷新，避免下拉框卡顿（activePopupWidget + 视图可见性双保险）
    if (QApplication::activePopupWidget()
        || m_Ui->dbComboBox->view()->isVisible()
        || m_Ui->userComboBox->view()->isVisible()
        || m_Ui->tunnelComboBox->view()->isVisible())
    {
        return;
    }
    runQuery();
}

void TrafficPage::runQuery()
{
    if (!m_DatabaseManager->isOpen())
    {
        m_TrafficModel->setRowCount(0);
        m_Ui->totalLabel->setText(QStringLiteral("总流量：-"));
        m_LastQuerySignature.clear();
        return;
    }

    QString dateFrom;
    QString dateTo;
    if (!m_Ui->allTimeCheck->isChecked())
    {
        dateFrom = m_Ui->startDatePicker->getSelectedDate().toString(QStringLiteral("yyyy-MM-dd"));
        dateTo = m_Ui->endDatePicker->getSelectedDate().toString(QStringLiteral("yyyy-MM-dd"));
        // 起始日期晚于结束日期时自动交换
        if (dateFrom > dateTo)
        {
            qSwap(dateFrom, dateTo);
        }
    }
    const QString rangeText = m_Ui->allTimeCheck->isChecked()
                                  ? QStringLiteral("全部时间")
                                  : QStringLiteral("%1 至 %2").arg(dateFrom, dateTo);

    const QString selectedTunnelName = m_Ui->tunnelComboBox->currentText();
    QString selectedTunnel = selectedTunnelName;
    if (selectedTunnel.endsWith(QStringLiteral("（已删除）")))
    {
        selectedTunnel.chop(QStringLiteral("（已删除）").size());
    }

    // 第一步：查询并收集结果（不触碰界面）
    struct Row
    {
        QString user;
        QString tunnel;
        qint64 bytesIn = 0;
        qint64 bytesOut = 0;
    };
    QList<Row> rows;
    QString totalText;

    if (m_CurrentUserId == kAllUsersId)
    {
        const QList<DatabaseManager::TrafficSummary> summaries =
            m_DatabaseManager->queryUserTraffic(dateFrom, dateTo);
        for (const DatabaseManager::TrafficSummary& summary : summaries)
        {
            rows.append(Row{summary.name, QStringLiteral("（全部）"),
                            summary.bytesIn, summary.bytesOut});
        }
        totalText = QStringLiteral("总流量（%1，%2 位用户）：%3")
                        .arg(rangeText)
                        .arg(summaries.size())
                        .arg(formatBytes(summaryTotal(summaries)));
    }
    else
    {
        const QList<DatabaseManager::TrafficSummary> summaries =
            m_DatabaseManager->queryTunnelTraffic(m_CurrentUserId, dateFrom, dateTo);
        const DatabaseManager::TrafficSummary userTotal =
            m_DatabaseManager->queryUserTotalTraffic(m_CurrentUserId);
        const QString userName = m_Ui->userComboBox->currentText();

        qint64 rangeTotal = 0;
        for (const DatabaseManager::TrafficSummary& summary : summaries)
        {
            if (!selectedTunnel.isEmpty() && selectedTunnel != QStringLiteral("全部隧道")
                && summary.name != selectedTunnel)
            {
                continue;
            }
            rows.append(Row{userName, summary.name, summary.bytesIn, summary.bytesOut});
            rangeTotal += summary.bytesIn + summary.bytesOut;
        }
        totalText = QStringLiteral("总流量（%1）：%2 | 历史总流量：%3")
                        .arg(rangeText)
                        .arg(formatBytes(rangeTotal))
                        .arg(formatBytes(userTotal.bytesIn + userTotal.bytesOut));
    }

    // 第二步：结果签名未变化则跳过表格重建（避免轮询/下拉交互时卡顿）
    QString signature;
    for (const Row& row : rows)
    {
        signature += QStringLiteral("%1|%2|%3|%4;")
                         .arg(row.user, row.tunnel)
                         .arg(row.bytesIn)
                         .arg(row.bytesOut);
    }
    signature += QStringLiteral("|") + totalText;
    if (signature == m_LastQuerySignature)
    {
        return;
    }
    m_LastQuerySignature = signature;

    // 第三步：增量更新表格——行结构不变时只改数值变化的单元格，
    // 保持滚动位置与选择，避免每 5 秒整表重建导致的卡顿
    const int oldRowCount = m_TrafficModel->rowCount();
    if (rows.size() < oldRowCount)
    {
        m_TrafficModel->removeRows(rows.size(), oldRowCount - rows.size());
    }
    while (m_TrafficModel->rowCount() < rows.size())
    {
        m_TrafficModel->insertRow(m_TrafficModel->rowCount());
    }
    auto setCell = [this](int row, int column, const QString& text) {
        QStandardItem* item = m_TrafficModel->item(row, column);
        if (item && item->text() == text)
        {
            return; // 值未变化，不触发重绘
        }
        if (!item)
        {
            item = new QStandardItem();
            m_TrafficModel->setItem(row, column, item);
        }
        item->setText(text);
        item->setTextAlignment(Qt::AlignCenter);
    };
    for (int i = 0; i < rows.size(); ++i)
    {
        const Row& row = rows[i];
        setCell(i, 0, row.user);
        setCell(i, 1, row.tunnel);
        setCell(i, 2, formatBytes(row.bytesIn));
        setCell(i, 3, formatBytes(row.bytesOut));
        setCell(i, 4, formatBytes(row.bytesIn + row.bytesOut));
    }
    m_Ui->totalLabel->setText(totalText);
}
