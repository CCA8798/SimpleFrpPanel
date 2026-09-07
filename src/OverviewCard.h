#ifndef OVERVIEWCARD_H
#define OVERVIEWCARD_H

#include <QColor>
#include <QList>
#include <QString>
#include <QWidget>

#include "ElaIcon.h"

// 首页"运行总览"卡片：QPainter 自绘（图标 + 标题 + 分隔线 + 键值行 + 状态圆点），
// 背景/边框/文字颜色全部取自 Ela 主题色板，随深浅主题自动切换。
class OverviewCard : public QWidget
{
    Q_OBJECT

public:
    // 一行内容：key 为左侧标签（弱化色），value 为右侧取值；
    // hasDot 时在行首画彩色状态圆点（如 frps 运行/停止）
    struct Row
    {
        QString key;
        QString value;
        QColor valueColor; // 无效 QColor 时使用主题文本色
        bool hasDot = false;
        QColor dotColor;
    };

    explicit OverviewCard(QWidget* parent = nullptr);

    void setCardIcon(ElaIconType::IconName icon);
    void setTitle(const QString& title);
    void setRows(const QList<Row>& rows);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_Title;
    ElaIconType::IconName m_Icon = ElaIconType::None;
    QList<Row> m_Rows;
};

#endif // OVERVIEWCARD_H
