#include "components/Wire.h"

#include "components/BaseComponent.h"

#include <QColor>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QString>

namespace {
QString wireTypeToString(WireType type)
{
    switch (type) {
    case WireType::Power:
        return "power";
    case WireType::Ground:
        return "ground";
    case WireType::Optical:
        return "optical";
    case WireType::Mechanical:
        return "mechanical";
    case WireType::Signal:
    default:
        return "signal";
    }
}

QColor wireColor(WireType type)
{
    switch (type) {
    case WireType::Power:
        return QColor(208, 50, 50);
    case WireType::Ground:
        return QColor(60, 70, 80);
    case WireType::Optical:
        return QColor(230, 140, 30);
    case WireType::Mechanical:
        return QColor(80, 120, 90);
    case WireType::Signal:
    default:
        return QColor(28, 100, 242);
    }
}
} // namespace

Wire::Wire(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
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
    const bool selected = isSelected();
    painter->setPen(QPen(wireColor(wireType), selected ? 3.0 : 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    for (int i = 1; i < points.size(); ++i) {
        painter->drawLine(points[i - 1], points[i]);
    }
}

void Wire::setEndpoints(
    BaseComponent* startComponent,
    ConnectionPad* startPad,
    BaseComponent* endComponent,
    ConnectionPad* endPad)
{
    detachFromPads();
    this->startComponent = startComponent;
    this->startPad = startPad;
    this->endComponent = endComponent;
    this->endPad = endPad;
    attachToPads();
    reroute();
}

void Wire::setPoints(const QList<QPointF>& points)
{
    prepareGeometryChange();
    this->points = points;
}

void Wire::attachToPads()
{
    if (startPad != nullptr && !startPad->connectedWires.contains(this)) {
        startPad->connectedWires.append(this);
    }
    if (endPad != nullptr && !endPad->connectedWires.contains(this)) {
        endPad->connectedWires.append(this);
    }
}

void Wire::detachFromPads()
{
    if (startPad != nullptr) {
        startPad->connectedWires.removeAll(this);
    }
    if (endPad != nullptr) {
        endPad->connectedWires.removeAll(this);
    }
}

void Wire::reroute()
{
    if (startComponent == nullptr || startPad == nullptr || endComponent == nullptr || endPad == nullptr) {
        return;
    }

    const QPointF startPoint = startComponent->mapToScene(startPad->localPos);
    const QPointF endPoint = endComponent->mapToScene(endPad->localPos);
    const double midX = (startPoint.x() + endPoint.x()) / 2.0;
    setPoints({
        startPoint,
        QPointF(midX, startPoint.y()),
        QPointF(midX, endPoint.y()),
        endPoint,
    });
}

QJsonObject Wire::toJson() const
{
    QJsonObject object;
    object["wireType"] = wireTypeToString(wireType);
    if (startComponent != nullptr && startPad != nullptr) {
        object["startComponentId"] = startComponent->id;
        object["startPadId"] = startPad->padId;
    }
    if (endComponent != nullptr && endPad != nullptr) {
        object["endComponentId"] = endComponent->id;
        object["endPadId"] = endPad->padId;
    }
    return object;
}
