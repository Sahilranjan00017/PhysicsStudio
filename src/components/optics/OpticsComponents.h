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

// ---------------------------------------------------------------------------
// PrismComponent  (OPT_PRISM)
// Equilateral triangular prism.  Rays refract at entry and exit faces using
// Snell's law with Cauchy dispersion: n(λ) = A + B/λ².
// Properties: sideLength (px), apexAngle (deg), cauchy_A, cauchy_B
// ---------------------------------------------------------------------------
class PrismComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit PrismComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// FilterComponent  (OPT_FILTER)
// Coloured optical filter — only passes wavelengths within a band-pass window.
// Rays outside [centerWavelength ± bandwidth/2] are absorbed.
// Properties: centerWavelength (nm), bandwidth (nm), transmittance (0–1)
// ---------------------------------------------------------------------------
class FilterComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit FilterComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// SlitComponent  (OPT_SLIT)
// Single or double slit aperture.  Only rays passing through the open slit
// gap(s) continue; all others are blocked.
// Properties: slitWidth (px), slitSeparation (px), numSlits (1 or 2),
//             screenLength (px)
// ---------------------------------------------------------------------------
class SlitComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit SlitComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// ConcaveMirrorComponent  (OPT_CONCAVE)
// Curved mirror: concave (focalLength > 0, converging) or convex (< 0).
// Paraxial reflection model: effective normal tilted by h/(2f) from vertex.
// Surface runs along the item's local X axis.
// Properties: focalLength (px), length (px), reflectivity (0–1)
// ---------------------------------------------------------------------------
class ConcaveMirrorComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit ConcaveMirrorComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// DiffractionGratingComponent  (OPT_GRATING)
// Transmission grating: splits rays into diffraction orders m = 0, ±1, ±2.
// Grating equation: sin(θ_m) = sin(θ_i) + m·λ/d.
// Surface runs along the item's local Y axis.
// Properties: gratingSpacing (nm), length (px), numOrders (1–3), transmittance
// ---------------------------------------------------------------------------
class DiffractionGratingComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit DiffractionGratingComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// BeamSplitterComponent  (OPT_SPLITTER)
// Partial mirror: reflects fraction R and transmits fraction T = 1 − R.
// Place at 45° to split a beam into two perpendicular paths.
// Surface runs along the item's local X axis.
// Properties: reflectance (0–1), length (px)
// ---------------------------------------------------------------------------
class BeamSplitterComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit BeamSplitterComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

// ---------------------------------------------------------------------------
// PolariserComponent  (OPT_POLARISER)
// Linear polarising filter.  Implements Malus's law:
//   Unpolarised input → I_out = I_in / 2   (always)
//   Polarised input   → I_out = I_in · cos²(Δθ)
// Outgoing ray carries the polariser's axis angle for downstream Malus calculations.
// Surface runs along the item's local Y axis.
// Properties: angle (polarisation axis, deg from +X), length (px), transmittance
// ---------------------------------------------------------------------------
class PolariserComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit PolariserComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
};

void registerOpticsComponents(ComponentRegistry& registry);
