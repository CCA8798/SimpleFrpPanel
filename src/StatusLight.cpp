#include "StatusLight.h"

#include <QPainter>

StatusLight::StatusLight(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(14, 14);
}

void StatusLight::setColor(const QColor& color)
{
    m_Color = color;
    update();
}

QColor StatusLight::color() const
{
    return m_Color;
}

void StatusLight::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_Color);
    painter.drawEllipse(rect().adjusted(2, 2, -2, -2));
}
