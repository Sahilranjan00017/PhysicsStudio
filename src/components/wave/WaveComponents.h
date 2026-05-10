#pragma once

#include "components/BaseComponent.h"

#include <QRectF>

class ComponentRegistry;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// WaveSourceComponent
// Emits a circular harmonic wave ψ = A·sin(ωt − kr + φ).
// Multiple sources produce interference patterns visible in the wave overlay.
// Properties : frequency (Hz), amplitude, phase (deg)
// simState   : simTime (set by solver — used to animate the concentric rings)
// ---------------------------------------------------------------------------
class WaveSourceComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveSourceComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// WaveDetectorComponent
// Samples the superposed wave amplitude at its scene position each tick.
// Useful for measuring path-length difference in interference experiments.
// simState : amplitude (live float, set by solver)
// ---------------------------------------------------------------------------
class WaveDetectorComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveDetectorComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

void registerWaveComponents(ComponentRegistry& registry);
