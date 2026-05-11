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

// ---------------------------------------------------------------------------
// WaveBarrierComponent  (WAV_BARRIER)
// Opaque barrier with one or two slits.
// In the wave solver, slit positions become Huygens secondary point sources.
// Properties : barrierLength (px), numSlits (1 or 2), slitWidth (px),
//              slitSeparation (px), frequency (Hz), amplitude
// simState   : (reads sources' frequency/amplitude from properties)
// ---------------------------------------------------------------------------
class WaveBarrierComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveBarrierComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// WaveDetectorComponent  (WAV_SOUND)
// Sound speaker — visual representation of a sound source.
// Stamped identically to WAV_SRC in the wave solver; the icon differentiates it.
// Properties : frequency (Hz), amplitude, phase (deg)
// ---------------------------------------------------------------------------
class WaveSoundComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveSoundComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

void registerWaveComponents(ComponentRegistry& registry);
