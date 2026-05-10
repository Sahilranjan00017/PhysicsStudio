#pragma once

#include "components/BaseComponent.h"

#include <QRectF>
#include <array>

class ComponentRegistry;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// BallComponent
// Point mass rendered as a filled circle.
// Properties : mass (kg), radius (px), restitution, velocityX, velocityY
// simState   : vx, vy, speed (written each tick by MotionSolver)
// ---------------------------------------------------------------------------
class BallComponent final : public BaseComponent {
    Q_OBJECT

public:
    explicit BallComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    ConnectionPad centerPad;
};

// ---------------------------------------------------------------------------
// BlockComponent
// Axis-aligned rigid rectangle.  Set mass = 0 (or property "fixed" = true)
// to make it immovable — useful as a wall or ramp.
// Properties : mass (kg), halfW (px), halfH (px), restitution, fixed (bool)
// ---------------------------------------------------------------------------
class BlockComponent final : public BaseComponent {
    Q_OBJECT

public:
    explicit BlockComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    std::array<ConnectionPad, 4> padStorage;  // top, right, bottom, left
};

// ---------------------------------------------------------------------------
// SpringComponent
// Hooke's-law force element between two mechanical pads.
// Renders as a coil spring zigzag.  When simulation is running the spring
// stretches visually to connect the two moving bodies.
// Properties : stiffness (N/m), restLength (px), damping (N·s/m)
// simState   : endAx, endAy, endBx, endBy, hasLiveEndpoints (set by solver)
// ---------------------------------------------------------------------------
class SpringComponent final : public BaseComponent {
    Q_OBJECT

public:
    explicit SpringComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    std::array<ConnectionPad, 2> padStorage;  // "a" and "b" endpoints
};

// ---------------------------------------------------------------------------
// AnchorComponent
// Immovable fixed point — springs can attach to it to simulate wall-mounted
// or ceiling-mounted springs.
// ---------------------------------------------------------------------------
class AnchorComponent final : public BaseComponent {
    Q_OBJECT

public:
    explicit AnchorComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    ConnectionPad anchorPad;
};

// Registers Ball, Block, Spring, Anchor with the ComponentRegistry.
void registerMotionComponents(ComponentRegistry& registry);
