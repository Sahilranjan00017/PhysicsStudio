#pragma once

#include "simulation/SolverInterfaces.h"

#include <limits>

#include <QList>
#include <QPointF>

class BaseComponent;

// ---------------------------------------------------------------------------
// OpticalSegment
// One straight piece of a traced ray.  The solver fills a list of these
// each tick; the OpticsOverlay renders them coloured by wavelength.
// ---------------------------------------------------------------------------
struct OpticalSegment {
    QPointF start;
    QPointF end;
    double  wavelength        = 550.0;   // nm — maps to visible colour
    double  intensity         = 1.0;     // 0–1 — maps to opacity / brightness
    double  polarisationAngle = std::numeric_limits<double>::quiet_NaN();
                                         // rad — NaN = unpolarised
};

// ---------------------------------------------------------------------------
// OpticalDomain
// Snapshot of all optical components in the scene plus the ray paths
// produced by the most recent solver pass.
// ---------------------------------------------------------------------------
struct OpticalDomain {
    QList<BaseComponent*>  components;   // sources, mirrors, lenses, screens
    QList<OpticalSegment>  segments;     // filled by OpticalSolver::trace()
};

// ---------------------------------------------------------------------------
// OpticalSolver
// Geometric (ray-optics) tracer.  Called once per tick (dynamic) and also
// on-demand after domain rebuild to show a static preview when paused.
//
// Supported elements (identified by typeId):
//   OPT_SRC      — LightSourceComponent        — emits rays
//   OPT_MIRROR   — MirrorComponent             — specular reflection
//   OPT_LENS     — LensComponent               — thin-lens refraction (paraxial)
//   OPT_SCREEN   — LightScreenComponent        — records hit positions in simState
//   OPT_PRISM    — PrismComponent              — Snell + Cauchy dispersion
//   OPT_FILTER   — FilterComponent             — wavelength bandpass filter
//   OPT_SLIT     — SlitComponent               — single / double slit aperture
//   OPT_CONCAVE  — ConcaveMirrorComponent      — curved mirror (paraxial)
//   OPT_GRATING  — DiffractionGratingComponent — grating equation, multi-order
//   OPT_SPLITTER — BeamSplitterComponent       — partial reflection + transmission
//   OPT_POLARISER— PolariserComponent          — Malus's law polarisation
// ---------------------------------------------------------------------------
class OpticalSolver final : public IOpticsSolver {
public:
    void trace(OpticalDomain& domain) override;

private:
    static constexpr int    MAX_BOUNCES  = 10;
    static constexpr double RAY_EPSILON  = 1e-4;   // minimum t to avoid self-hit
    static constexpr double MAX_RAY_LEN  = 3000.0; // px — hard cap per segment

    // Sentinel for unpolarised rays.
    static constexpr double UNPOLARISED = std::numeric_limits<double>::quiet_NaN();

    // Trace one ray and append segments to domain.segments.
    // nMedium: current refractive index (1.0 = air).
    // polAngle: polarisation angle in radians; NaN = unpolarised.
    static void traceRay(OpticalDomain& domain,
                         QPointF pos, QPointF dir,
                         double wavelength, double intensity,
                         double remainingLength, int bounceCount,
                         const QList<BaseComponent*>& blockers,
                         double nMedium  = 1.0,
                         double polAngle = std::numeric_limits<double>::quiet_NaN());

    // Return all scene-space surface segments for a component.
    // Single-surface → one entry; prism → two.
    static QList<QPair<QPointF,QPointF>> allSurfaces(BaseComponent* comp);

    // Return the two scene-space endpoints of a component's primary surface.
    static bool surfaceEndpoints(BaseComponent* comp,
                                 QPointF& outA, QPointF& outB);

    // 2D parametric ray-segment intersection.
    static bool intersectSegment(QPointF rayP, QPointF rayD,
                                 QPointF segA, QPointF segB,
                                 double& t, QPointF& hitPt, QPointF& normal);

    // Thin-lens angular deflection.
    static QPointF thinLensRefract(QPointF dir, QPointF normal,
                                   const QPointF& hitPt, BaseComponent* lens);

    // Curved mirror paraxial reflection (concave / convex).
    static QPointF curvedMirrorReflect(QPointF dir, QPointF flatNormal,
                                       const QPointF& hitPt, BaseComponent* mirror);

    // Snell's law vector refraction (returns zero on TIR).
    static QPointF snellRefract(QPointF dir, QPointF normal, double n1, double n2);

    // Cauchy dispersion: n(λ) = A + B/λ² (λ in nm).
    static double cauchyIndex(double lambda_nm, double A, double B);

    // Returns true if the hitPt falls inside a slit gap.
    static bool slitPassesRay(BaseComponent* slit, const QPointF& hitPt);

    // Write a hit position to the screen's simState.
    static void recordScreenHit(BaseComponent* screen, const QPointF& hitPt);
};
