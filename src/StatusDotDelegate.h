#ifndef STATUSDOTDELEGATE_H
#define STATUSDOTDELEGATE_H

#include <QStyledItemDelegate>

// 隧道"运行状况"列委托：彩色状态灯 + 文本
//   运行中 = 绿色 / 未运行 = 灰色 / 已禁用 = 橙色
class StatusDotDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit StatusDotDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif // STATUSDOTDELEGATE_H
