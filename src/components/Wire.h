#pragma once

#include "components/ConnectionPad.h"

#include <QGraphicsItem>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QVariant>

class BaseComponent;

enum class WireType {
    Power,
    Ground,
    Signal,
    Optical,
    Mechanical
};

class Wire final : public QGraphicsItem {
public:
    explicit Wire(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    void setEndpoints(
        BaseComponent* startComponent,
        ConnectionPad* startPad,
        BaseComponent* endComponent,
        ConnectionPad* endPad);
    void setPoints(const QList<QPointF>& points);
    void attachToPads();
    void detachFromPads();
    void reroute();
    QJsonObject toJson() const;

    QList<QPointF> points;
    BaseComponent* startComponent = nullptr;
    BaseComponent* endComponent = nullptr;
    ConnectionPad* startPad = nullptr;
    ConnectionPad* endPad = nullptr;
    QVariant signal;
    WireType wireType = WireType::Signal;
};
