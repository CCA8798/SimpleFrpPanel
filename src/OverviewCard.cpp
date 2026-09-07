#include "OverviewCard.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include "ElaTheme.h"

namespace {
const int kMargin = 18;        // 卡内容左右留白
const int kTopMargin = 14;     // 卡内容顶部留白
const int kTitleIconSize = 18; // 标题行图标尺寸
const int kTitleSpacing = 10;  // 图标与标题间距
const int kHeaderBottom = 12;  // 标题行底部到分隔线的距离
const int kSeparatorGap = 10;  // 分隔线到首行的距离
const int kRowHeight = 26;     // 键值行高
const int kDotRadius = 4;      // 状态圆点半径
const int kKeyValueGap = 6;    // key 与 value 间距
const int kTextDotGap = 8;     // 圆点与文本间距
} // namespace

OverviewCard::OverviewCard(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(120);
}

void OverviewCard::setCardIcon(ElaIconType::IconName icon)
{
    m_Icon = icon;
    update();
}

void OverviewCard::setTitle(const QString& title)
{
    m_Title = title;
    update();
}

void OverviewCard::setRows(const QList<Row>& rows)
{
    m_Rows = rows;
    update();
}

QSize OverviewCard::sizeHint() const
{
    const int height = kTopMargin + kTitleIconSize + kHeaderBottom + kSeparatorGap
                       + m_Rows.size() * kRowHeight + 16;
    return QSize(300, qMax(height, 150));
}

void OverviewCard::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    const ElaThemeType::ThemeMode theme = eTheme->getThemeMode();
    const QColor background = ElaThemeColor(theme, BasicBaseDeep);
    const QColor border = ElaThemeColor(theme, BasicBorder);
    const QColor titleColor = ElaThemeColor(theme, BasicText);
    const QColor keyColor = ElaThemeColor(theme, BasicDetailsText);
    const QColor valueColor = ElaThemeColor(theme, BasicText);
    const QColor accentColor = ElaThemeColor(theme, PrimaryNormal);
    const QColor dotGray(0x9E, 0x9E, 0x9E);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    // 卡片背景与圆角边框
    const QRectF cardRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(cardRect, 10, 10);
    painter.fillPath(path, background);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);

    int y = kTopMargin;

    // 标题行：图标 + 标题
    int titleX = kMargin;
    if (m_Icon != ElaIconType::None)
    {
        const QIcon icon = ElaIcon::getInstance()->getElaIcon(m_Icon, kTitleIconSize, accentColor);
        painter.drawPixmap(kMargin, y, icon.pixmap(kTitleIconSize, kTitleIconSize));
        titleX = kMargin + kTitleIconSize + kTitleSpacing;
    }
    QFont titleFont = painter.font();
    titleFont.setPixelSize(15);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(titleColor);
    painter.drawText(QRect(titleX, y, width() - titleX - kMargin, kTitleIconSize),
                     Qt::AlignLeft | Qt::AlignVCenter, m_Title);

    y += kTitleIconSize + kHeaderBottom;

    // 分隔线
    painter.setPen(QPen(border, 1));
    painter.drawLine(kMargin, y, width() - kMargin, y);
    y += kSeparatorGap;

    // 键值行
    QFont rowFont = painter.font();
    rowFont.setPixelSize(13);
    rowFont.setWeight(QFont::Normal);
    for (const Row& row : m_Rows)
    {
        const int rowCenterY = y + kRowHeight / 2;
        int textX = kMargin;

        if (row.hasDot)
        {
            const QColor dotColor = row.dotColor.isValid() ? row.dotColor : dotGray;
            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);
            painter.drawEllipse(QPointF(kMargin + kDotRadius, rowCenterY),
                                kDotRadius, kDotRadius);
            textX += 2 * kDotRadius + kTextDotGap;
        }

        painter.setFont(rowFont);
        painter.setPen(keyColor);
        const QString keyText = row.key + tr("：");
        const int keyWidth = painter.fontMetrics().horizontalAdvance(keyText);
        painter.drawText(QRect(textX, y, keyWidth, kRowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, keyText);
        textX += keyWidth + kKeyValueGap;

        painter.setPen(row.valueColor.isValid() ? row.valueColor : valueColor);
        const int remainWidth = width() - textX - kMargin;
        if (remainWidth > 10)
        {
            const QString elided = painter.fontMetrics()
                                       .elidedText(row.value, Qt::ElideRight, remainWidth);
            painter.drawText(QRect(textX, y, remainWidth, kRowHeight),
                             Qt::AlignLeft | Qt::AlignVCenter, elided);
        }
        y += kRowHeight;
    }
}
