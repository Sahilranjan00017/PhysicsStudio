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

// ---------------------------------------------------------------------------
// WaveWallComponent  (WAV_WALL)
// Hard reflective boundary.  Image-source method: each primary source is
// mirrored across the wall line with a 180° phase shift (Dirichlet condition),
// producing destructive cancellation at the wall and standing-wave patterns.
// Properties: length (px), reflectance (0–1)
// ---------------------------------------------------------------------------
class WaveWallComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveWallComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// WavePlaneSourceComponent  (WAV_PLANE)
// Emits a planar (linear) wavefront by stamping a row of coherent point
// sources spaced ≈ 10 px apart along the component's local Y axis.
// Rotation sets the propagation direction (emission is toward local +X).
// Properties: frequency (Hz), amplitude, phase (deg), width (px)
// ---------------------------------------------------------------------------
class WavePlaneSourceComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WavePlaneSourceComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// WaveAbsorberComponent  (WAV_ABSORBER)
// Anechoic region: damps the field within a circular radius after each step.
// Field is multiplied by (1 − damping·falloff) where falloff = 1 at the
// centre and tapers linearly to 0 at the edge.
// Properties: radius (px), damping (0–1, fraction removed per tick)
// ---------------------------------------------------------------------------
class WaveAbsorberComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveAbsorberComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// WaveRippleComponent  (WAV_RIPPLE)
// Finite-duration pulse source — like a stone dropped in a ripple tank.
// Amplitude envelope: sin(π·t/T) for 0 ≤ t ≤ T = numCycles/freq, then 0.
// Properties: frequency (Hz), amplitude, phase (deg), numCycles
// simState  : simTime (for visual animation)
// ---------------------------------------------------------------------------
class WaveRippleComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit WaveRippleComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

void registerWaveComponents(ComponentRegistry& registry);
