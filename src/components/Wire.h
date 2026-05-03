#pragma once

#include "components/ConnectionPad.h"

#include <QGraphicsItem>
#include <QList>
#include <QPointF>
#include <QVariant>

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
    void reroute();

    QList<QPointF> points;
    ConnectionPad* startPad = nullptr;
    ConnectionPad* endPad = nullptr;
    QVariant signal;
    WireType wireType = WireType::Signal;
};

