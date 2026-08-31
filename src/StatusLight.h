#ifndef STATUSLIGHT_H
#define STATUSLIGHT_H

#include <QColor>
#include <QWidget>

// 状态灯：一个可切换颜色的小圆点指示器（绿=运行中 / 灰=未运行 / 橙=已禁用）
class StatusLight : public QWidget
{
    Q_OBJECT

public:
    explicit StatusLight(QWidget* parent = nullptr);

    void setColor(const QColor& color);
    QColor color() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_Color = QColor(0x9E, 0x9E, 0x9E);
};

#endif // STATUSLIGHT_H
