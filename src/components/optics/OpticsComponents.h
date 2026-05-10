#pragma once

#include "components/BaseComponent.h"

#include <QRectF>

class ComponentRegistry;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// LightSourceComponent
// Emits one or more parallel or spread rays in a configurable direction.
// Properties : angle (deg, 0=right/+x, 90=down/+y), spread (deg),
//              numRays (1–20), intensity (0–1), wavelength (nm), rayLength (px)
// ---------------------------------------------------------------------------
class LightSourceComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit LightSourceComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// MirrorComponent
// Flat specular mirror.  Rotate the component to set the reflection angle.
// The reflective surface extends ±length/2 along the item's local X axis.
// Properties : length (px), reflectivity (0–1)
// ---------------------------------------------------------------------------
class MirrorComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit MirrorComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// LensComponent
// Thin-lens: converging (f>0) or diverging (f<0).
// The lens surface extends ±diameter/2 along the item's local Y axis.
// The optical axis is the item's local X axis.
// Properties : focalLength (px, +converging / −diverging), diameter (px),
//              transmittance (0–1)
// ---------------------------------------------------------------------------
class LensComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit LensComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// LightScreenComponent
// Projection screen — shows coloured dots wherever rays land.
// The screen surface extends ±length/2 along the item's local Y axis.
// Properties : length (px)
// simState   : hitCount, hit0x, hit0y, hit1x, hit1y, … (set by solver)
// ---------------------------------------------------------------------------
class LightScreenComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit LightScreenComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

void registerOpticsComponents(ComponentRegistry& registry);
