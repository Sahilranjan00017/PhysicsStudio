#pragma once

#include "simulation/SolverInterfaces.h"

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
    double  wavelength = 550.0;   // nm — maps to visible colour
    double  intensity  = 1.0;     // 0–1 — maps to opacity / brightness
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
//   OPT_SRC    — LightSourceComponent — emits rays
//   OPT_MIRROR — MirrorComponent      — specular reflection
//   OPT_LENS   — LensComponent        — thin-lens refraction (paraxial model)
//   OPT_SCREEN — LightScreenComponent — records hit positions in simState
//   OPT_PRISM  — PrismComponent       — Snell refraction + Cauchy dispersion
//   OPT_FILTER — FilterComponent      — wavelength bandpass filter
//   OPT_SLIT   — SlitComponent        — aperture with one or two slits
// ---------------------------------------------------------------------------
class OpticalSolver final : public IOpticsSolver {
public:
    void trace(OpticalDomain& domain) override;

private:
    static constexpr int    MAX_BOUNCES  = 8;
    static constexpr double RAY_EPSILON  = 1e-4;   // minimum t to avoid self-hit
    static constexpr double MAX_RAY_LEN  = 3000.0; // px — hard cap per segment

    // Trace one ray and append segments to domain.segments.
    // nMedium: current refractive index of the medium the ray is in (1.0 = air).
    static void traceRay(OpticalDomain& domain,
                         QPointF pos, QPointF dir,
                         double wavelength, double intensity,
                         double remainingLength, int bounceCount,
                         const QList<BaseComponent*>& blockers,
                         double nMedium = 1.0);

    // Return all scene-space surface segments for a component.
    // Single-surface components return one entry; prism returns two.
    static QList<QPair<QPointF,QPointF>> allSurfaces(BaseComponent* comp);

    // Return the two scene-space endpoints of a component's primary surface.
    static bool surfaceEndpoints(BaseComponent* comp,
                                 QPointF& outA, QPointF& outB);

    // 2D parametric ray-segment intersection.
    // Returns true + fills t (along ray), hitPt, normal if intersected.
    static bool intersectSegment(QPointF rayP, QPointF rayD,
                                 QPointF segA, QPointF segB,
                                 double& t, QPointF& hitPt, QPointF& normal);

    // Thin-lens angular deflection.
    static QPointF thinLensRefract(QPointF dir, QPointF normal,
                                   const QPointF& hitPt, BaseComponent* lens);

    // Snell's law vector refraction.
    // n1: incoming medium, n2: outgoing medium.
    // normal must point from medium1 into medium2 (opposes incoming ray).
    // Returns refracted direction (zero vector on total internal reflection).
    static QPointF snellRefract(QPointF dir, QPointF normal, double n1, double n2);

    // Cauchy dispersion: n(λ) = A + B/λ² (λ in nm).
    static double cauchyIndex(double lambda_nm, double A, double B);

    // Check if the hitPt on a slit component falls inside a gap (returns true = passes through).
    static bool slitPassesRay(BaseComponent* slit, const QPointF& hitPt);

    // Write a hit position to the screen's simState.
    static void recordScreenHit(BaseComponent* screen, const QPointF& hitPt);
};
