#include "components/Wire.h"

#include <QPainter>

Wire::Wire(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
}

QRectF Wire::boundingRect() const
{
    if (points.isEmpty()) {
        return QRectF();
    }

    QRectF bounds(points.first(), QSizeF(1, 1));
    for (const QPointF& point : points) {
        bounds = bounds.united(QRectF(point, QSizeF(1, 1)));
    }
    return bounds.adjusted(-6, -6, 6, 6);
}

void Wire::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (points.size() < 2) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    for (int i = 1; i < points.size(); ++i) {
        painter->drawLine(points[i - 1], points[i]);
    }
}

void Wire::reroute()
{
    prepareGeometryChange();
    // Manhattan routing will be introduced with the interaction engine.
}

