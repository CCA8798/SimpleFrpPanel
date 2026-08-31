#include "StatusDotDelegate.h"

#include <QApplication>
#include <QPainter>

StatusDotDelegate::StatusDotDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void StatusDotDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    // 绘制默认的选中/悬停背景，但清空默认文本（否则文字会被绘制两次造成重叠）
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QString text = index.data(Qt::DisplayRole).toString();

    // 状态灯颜色
    QColor dotColor(0x9E, 0x9E, 0x9E); // 未运行：灰
    if (text == QStringLiteral("运行中"))
    {
        dotColor = QColor(0x4C, 0xAF, 0x50); // 绿
    }
    else if (text == QStringLiteral("已禁用"))
    {
        dotColor = QColor(0xFF, 0x98, 0x00); // 橙
    }

    const int dotDiameter = 10;
    const QRect dotRect(opt.rect.left() + 8, opt.rect.center().y() - dotDiameter / 2,
                        dotDiameter, dotDiameter);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(dotColor);
    painter->drawEllipse(dotRect);

    // 状态文本
    QRect textRect = opt.rect;
    textRect.setLeft(dotRect.right() + 6);
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    painter->restore();
}

QSize StatusDotDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // 为状态灯留出空间
    size.setWidth(size.width() + 26);
    return size;
}
