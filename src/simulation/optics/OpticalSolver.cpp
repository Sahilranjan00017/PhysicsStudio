#include "simulation/optics/OpticalSolver.h"

#include "components/BaseComponent.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

inline double cross2d(QPointF a, QPointF b)
{
    return a.x() * b.y() - a.y() * b.x();
}

inline double dot2d(QPointF a, QPointF b)
{
    return a.x() * b.x() + a.y() * b.y();
}

inline double length2d(QPointF v)
{
    return std::sqrt(v.x() * v.x() + v.y() * v.y());
}

inline QPointF normalize2d(QPointF v)
{
    const double len = length2d(v);
    return (len > 1e-12) ? QPointF(v.x() / len, v.y() / len) : QPointF(1.0, 0.0);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// OpticalSolver::trace
// ---------------------------------------------------------------------------
void OpticalSolver::trace(OpticalDomain& domain)
{
    domain.segments.clear();

    // Partition components by role.
    QList<BaseComponent*> sources, blockers;
    for (auto* comp : domain.components) {
        if (comp->typeId == "OPT_SRC")
            sources  << comp;
        else
            blockers << comp;   // mirrors, lenses, screens are all blockers
    }

    // Reset screen hit counters.
    for (auto* comp : domain.components) {
        if (comp->typeId == "OPT_SCREEN") {
            comp->simState["hitCount"] = 0;
            comp->update();
        }
    }

    // Emit rays from each source.
    for (auto* src : sources) {
        const double angle      = src->properties.value("angle",      0.0).toDouble();
        const double spread     = src->properties.value("spread",     0.0).toDouble();
        const int    numRays    = std::clamp(src->properties.value("numRays", 1).toInt(), 1, 20);
        const double intensity  = src->properties.value("intensity",  1.0).toDouble();
        const double wavelength = src->properties.value("wavelength", 550.0).toDouble();
        const double rayLength  = src->properties.value("rayLength",  2000.0).toDouble();

        const QPointF srcPos    = src->pos();

        for (int i = 0; i < numRays; ++i) {
            double rayAngle = angle;
            if (numRays > 1)
                rayAngle = angle - spread * 0.5 + i * spread / static_cast<double>(numRays - 1);

            // Qt scene: y increases downward, angle 0 = right (+x), 90 = down (+y).
            const double rad = qDegreesToRadians(rayAngle);
            const QPointF dir(std::cos(rad), std::sin(rad));

            traceRay(domain, srcPos, dir, wavelength, intensity, rayLength, 0, blockers);
        }
    }
}

// ---------------------------------------------------------------------------
// OpticalSolver::traceRay (recursive)
// ---------------------------------------------------------------------------
void OpticalSolver::traceRay(OpticalDomain& domain,
                              QPointF pos, QPointF dir,
                              double wavelength, double intensity,
                              double remainingLength, int bounceCount,
                              const QList<BaseComponent*>& blockers)
{
    if (bounceCount > MAX_BOUNCES || intensity < 0.01 || remainingLength < 1.0)
        return;

    // Find the nearest blocking element.
    double        tMin   = remainingLength;
    BaseComponent* hitComp = nullptr;
    QPointF        hitPt, hitNormal;

    for (auto* blocker : blockers) {
        QPointF A, B;
        if (!surfaceEndpoints(blocker, A, B))
            continue;

        double  t;
        QPointF pt, normal;
        if (intersectSegment(pos, dir, A, B, t, pt, normal)
                && t > RAY_EPSILON && t < tMin) {
            tMin     = t;
            hitComp  = blocker;
            hitPt    = pt;
            hitNormal = normal;
        }
    }

    // Record this ray segment (to hit or to maximum reach).
    OpticalSegment seg;
    seg.start      = pos;
    seg.end        = hitComp ? hitPt : (pos + dir * remainingLength);
    seg.wavelength = wavelength;
    seg.intensity  = intensity;
    domain.segments << seg;

    if (!hitComp)
        return;

    const double consumed = tMin;   // length of this segment

    // Apply the optics of the hit element.
    if (hitComp->typeId == "OPT_MIRROR") {
        const double reflectivity = hitComp->properties.value("reflectivity", 0.95).toDouble();
        const QPointF reflected = normalize2d(dir - hitNormal * (2.0 * dot2d(dir, hitNormal)));
        traceRay(domain, hitPt, reflected, wavelength,
                 intensity * reflectivity,
                 remainingLength - consumed, bounceCount + 1, blockers);

    } else if (hitComp->typeId == "OPT_LENS") {
        const double transmittance = hitComp->properties.value("transmittance", 0.97).toDouble();
        const QPointF refracted = thinLensRefract(dir, hitNormal, hitPt, hitComp);
        traceRay(domain, hitPt, refracted, wavelength,
                 intensity * transmittance,
                 remainingLength - consumed, bounceCount + 1, blockers);

    } else if (hitComp->typeId == "OPT_SCREEN") {
        recordScreenHit(hitComp, hitPt);
        // Ray stops at the screen.
    }
}

// ---------------------------------------------------------------------------
// OpticalSolver::surfaceEndpoints
// Returns the two scene-space endpoints of a component's optical surface.
// Mirror surface: horizontal in local space (±halfLen on X axis).
// Lens / Screen:  vertical in local space (±halfLen on Y axis).
// ---------------------------------------------------------------------------
bool OpticalSolver::surfaceEndpoints(BaseComponent* comp, QPointF& outA, QPointF& outB)
{
    if (comp->typeId == "OPT_MIRROR") {
        const double half = comp->properties.value("length", 100.0).toDouble() * 0.5;
        outA = comp->mapToScene(QPointF(-half, 0.0));
        outB = comp->mapToScene(QPointF( half, 0.0));
        return true;
    }
    if (comp->typeId == "OPT_LENS") {
        const double half = comp->properties.value("diameter", 100.0).toDouble() * 0.5;
        outA = comp->mapToScene(QPointF(0.0, -half));
        outB = comp->mapToScene(QPointF(0.0,  half));
        return true;
    }
    if (comp->typeId == "OPT_SCREEN") {
        const double half = comp->properties.value("length", 120.0).toDouble() * 0.5;
        outA = comp->mapToScene(QPointF(0.0, -half));
        outB = comp->mapToScene(QPointF(0.0,  half));
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// OpticalSolver::intersectSegment
// 2D parametric intersection of ray P+t*D with segment A+s*(B-A), s∈[0,1].
// Returns true and fills t, hitPt, normal when valid intersection found.
// Normal is oriented to face the incoming ray (dot(normal, dir) < 0).
// ---------------------------------------------------------------------------
bool OpticalSolver::intersectSegment(QPointF rayP, QPointF rayD,
                                     QPointF segA, QPointF segB,
                                     double& t, QPointF& hitPt, QPointF& normal)
{
    const QPointF AB = segB - segA;           // segment direction vector
    const QPointF AP = segA - rayP;           // vector from ray origin to seg start

    const double det = cross2d(AB, rayD);     // AB × D
    if (std::abs(det) < 1e-10)
        return false;   // ray parallel to segment

    // Solve: t = cross2d(AB, AP) / det,   s = cross2d(D, AP) / det
    t            = cross2d(AB, AP) / det;
    const double s = cross2d(rayD, AP) / det;

    if (t < 0.0 || s < 0.0 || s > 1.0)
        return false;

    hitPt = rayP + rayD * t;

    // Normal = perpendicular to segment, oriented against the ray.
    const QPointF segDir = normalize2d(AB);
    normal = QPointF(-segDir.y(), segDir.x());
    if (dot2d(normal, rayD) > 0.0)
        normal = QPointF(-normal.x(), -normal.y());

    return true;
}

// ---------------------------------------------------------------------------
// OpticalSolver::thinLensRefract
// Thin-lens model (paraxial height-slope formula).
// Deflects the incoming ray direction based on its height from the
// lens optical axis and the focal length.
// ---------------------------------------------------------------------------
QPointF OpticalSolver::thinLensRefract(QPointF dir, QPointF normal,
                                       const QPointF& hitPt, BaseComponent* lens)
{
    const double f = lens->properties.value("focalLength", 150.0).toDouble();
    if (std::abs(f) < 1.0)
        return dir;   // degenerate lens — pass through unchanged

    // Forward direction through the lens (opposite to the face-normal).
    const QPointF fwd = QPointF(-normal.x(), -normal.y());
    // Tangent along the lens surface (perpendicular to forward).
    const QPointF tng = QPointF(-fwd.y(), fwd.x());

    // Height of intersection from the lens centre, measured along lens surface.
    const QPointF lensCenter = lens->pos();
    const double h = dot2d(hitPt - lensCenter, tng);

    // Decompose incoming direction into forward and tangential components.
    const double d_fwd = dot2d(dir, fwd);   // positive: going through lens
    const double d_tng = dot2d(dir, tng);   // signed height-slope

    // Thin-lens angular deflection: d_tng_new = d_tng - d_fwd * h / f.
    // Positive f → converging: ray at +h deflected towards axis.
    const double d_tng_new = d_tng - d_fwd * h / f;

    const QPointF newDir = fwd * d_fwd + tng * d_tng_new;
    return normalize2d(newDir);
}

// ---------------------------------------------------------------------------
// OpticalSolver::recordScreenHit
// Appends a hit position to the screen component's simState.
// Keys: "hitCount", "hit0x", "hit0y", "hit1x", "hit1y", …
// ---------------------------------------------------------------------------
void OpticalSolver::recordScreenHit(BaseComponent* screen, const QPointF& hitPt)
{
    constexpr int MAX_HITS = 64;
    const int count = screen->simState.value("hitCount", 0).toInt();
    if (count >= MAX_HITS)
        return;

    screen->simState[QString("hit%1x").arg(count)] = hitPt.x();
    screen->simState[QString("hit%1y").arg(count)] = hitPt.y();
    screen->simState["hitCount"] = count + 1;
    screen->update();
}
