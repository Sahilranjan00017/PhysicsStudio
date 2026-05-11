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
            blockers << comp;
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

        const QPointF srcPos = src->pos();

        for (int i = 0; i < numRays; ++i) {
            double rayAngle = angle;
            if (numRays > 1)
                rayAngle = angle - spread * 0.5 + i * spread / static_cast<double>(numRays - 1);

            const double rad = qDegreesToRadians(rayAngle);
            const QPointF dir(std::cos(rad), std::sin(rad));

            traceRay(domain, srcPos, dir, wavelength, intensity, rayLength, 0, blockers, 1.0);
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
                              const QList<BaseComponent*>& blockers,
                              double nMedium)
{
    if (bounceCount > MAX_BOUNCES || intensity < 0.01 || remainingLength < 1.0)
        return;

    // Find the nearest blocking element / surface.
    double         tMin     = remainingLength;
    BaseComponent* hitComp  = nullptr;
    QPointF        hitPt, hitNormal;

    for (auto* blocker : blockers) {
        for (const auto& [A, B] : allSurfaces(blocker)) {
            double  t;
            QPointF pt, normal;
            if (intersectSegment(pos, dir, A, B, t, pt, normal)
                    && t > RAY_EPSILON && t < tMin) {
                tMin      = t;
                hitComp   = blocker;
                hitPt     = pt;
                hitNormal = normal;
            }
        }
    }

    // Record this ray segment.
    OpticalSegment seg;
    seg.start      = pos;
    seg.end        = hitComp ? hitPt : (pos + dir * remainingLength);
    seg.wavelength = wavelength;
    seg.intensity  = intensity;
    domain.segments << seg;

    if (!hitComp)
        return;

    const double consumed = tMin;
    const QString& hitType = hitComp->typeId;

    // ── Mirror ──────────────────────────────────────────────────────────────
    if (hitType == "OPT_MIRROR") {
        const double reflectivity = hitComp->properties.value("reflectivity", 0.95).toDouble();
        const QPointF reflected = normalize2d(dir - hitNormal * (2.0 * dot2d(dir, hitNormal)));
        traceRay(domain, hitPt, reflected, wavelength,
                 intensity * reflectivity,
                 remainingLength - consumed, bounceCount + 1, blockers, nMedium);

    // ── Lens ─────────────────────────────────────────────────────────────────
    } else if (hitType == "OPT_LENS") {
        const double transmittance = hitComp->properties.value("transmittance", 0.97).toDouble();
        const QPointF refracted = thinLensRefract(dir, hitNormal, hitPt, hitComp);
        traceRay(domain, hitPt, refracted, wavelength,
                 intensity * transmittance,
                 remainingLength - consumed, bounceCount + 1, blockers, nMedium);

    // ── Screen ───────────────────────────────────────────────────────────────
    } else if (hitType == "OPT_SCREEN") {
        recordScreenHit(hitComp, hitPt);

    // ── Prism — Snell's law with Cauchy dispersion ────────────────────────
    } else if (hitType == "OPT_PRISM") {
        const double A = hitComp->properties.value("cauchy_A", 1.458).toDouble();
        const double B = hitComp->properties.value("cauchy_B", 3.54e6).toDouble();
        const double nGlass = cauchyIndex(wavelength, A, B);

        double n1, n2;
        if (nMedium < 1.01) {
            // Entering prism (in air → glass).
            n1 = 1.0;
            n2 = nGlass;
        } else {
            // Exiting prism (in glass → air).
            n1 = nMedium;
            n2 = 1.0;
        }

        const QPointF refracted = snellRefract(dir, hitNormal, n1, n2);
        if (refracted.x() == 0.0 && refracted.y() == 0.0) {
            // Total internal reflection.
            const QPointF reflected = normalize2d(dir - hitNormal * (2.0 * dot2d(dir, hitNormal)));
            traceRay(domain, hitPt, reflected, wavelength,
                     intensity * 0.95,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium);
        } else {
            traceRay(domain, hitPt, refracted, wavelength,
                     intensity * 0.98,
                     remainingLength - consumed, bounceCount + 1, blockers, n2);
        }

    // ── Filter — wavelength bandpass ─────────────────────────────────────
    } else if (hitType == "OPT_FILTER") {
        const double center = hitComp->properties.value("centerWavelength", 550.0).toDouble();
        const double bw     = hitComp->properties.value("bandwidth",         80.0).toDouble();
        const double trans  = hitComp->properties.value("transmittance",      0.90).toDouble();

        if (std::abs(wavelength - center) <= bw * 0.5) {
            // Within pass-band — continue ray (pass-through, same direction).
            const QPointF fwd = normalize2d(dir);
            traceRay(domain, hitPt, fwd, wavelength,
                     intensity * trans,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium);
        }
        // else: absorbed — ray stops.

    // ── Slit — aperture blocker ───────────────────────────────────────────
    } else if (hitType == "OPT_SLIT") {
        if (slitPassesRay(hitComp, hitPt)) {
            // Ray passes through the gap — continue in same direction.
            const QPointF fwd = normalize2d(dir);
            traceRay(domain, hitPt, fwd, wavelength,
                     intensity,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium);
        }
        // else: ray hits opaque region — absorbed.
    }
}

// ---------------------------------------------------------------------------
// OpticalSolver::allSurfaces
// Returns the scene-space endpoints of every optical face of a component.
// Prism: two faces (left and right refracting faces).
// All others: one face.
// ---------------------------------------------------------------------------
QList<QPair<QPointF,QPointF>> OpticalSolver::allSurfaces(BaseComponent* comp)
{
    QList<QPair<QPointF,QPointF>> result;

    if (comp->typeId == "OPT_PRISM") {
        const double s = comp->properties.value("sideLength", 100.0).toDouble();
        const double h = s * std::sqrt(3.0) / 2.0;
        // Apex at (0, -h/2), bottom-left (-s/2, h/2), bottom-right (s/2, h/2).
        const QPointF apex  = comp->mapToScene(QPointF(  0.0, -h * 0.5));
        const QPointF botL  = comp->mapToScene(QPointF(-s * 0.5,  h * 0.5));
        const QPointF botR  = comp->mapToScene(QPointF( s * 0.5,  h * 0.5));
        result << QPair<QPointF,QPointF>(apex, botL);  // left face
        result << QPair<QPointF,QPointF>(apex, botR);  // right face
        return result;
    }

    QPointF A, B;
    if (surfaceEndpoints(comp, A, B))
        result << QPair<QPointF,QPointF>(A, B);
    return result;
}

// ---------------------------------------------------------------------------
// OpticalSolver::surfaceEndpoints
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
    if (comp->typeId == "OPT_FILTER") {
        const double half = comp->properties.value("length", 80.0).toDouble() * 0.5;
        outA = comp->mapToScene(QPointF(0.0, -half));
        outB = comp->mapToScene(QPointF(0.0,  half));
        return true;
    }
    if (comp->typeId == "OPT_SLIT") {
        const double half = comp->properties.value("screenLength", 120.0).toDouble() * 0.5;
        outA = comp->mapToScene(QPointF(0.0, -half));
        outB = comp->mapToScene(QPointF(0.0,  half));
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// OpticalSolver::intersectSegment
// ---------------------------------------------------------------------------
bool OpticalSolver::intersectSegment(QPointF rayP, QPointF rayD,
                                     QPointF segA, QPointF segB,
                                     double& t, QPointF& hitPt, QPointF& normal)
{
    const QPointF AB = segB - segA;
    const QPointF AP = segA - rayP;

    const double det = cross2d(AB, rayD);
    if (std::abs(det) < 1e-10)
        return false;

    t            = cross2d(AB, AP) / det;
    const double s = cross2d(rayD, AP) / det;

    if (t < 0.0 || s < 0.0 || s > 1.0)
        return false;

    hitPt = rayP + rayD * t;

    const QPointF segDir = normalize2d(AB);
    normal = QPointF(-segDir.y(), segDir.x());
    if (dot2d(normal, rayD) > 0.0)
        normal = QPointF(-normal.x(), -normal.y());

    return true;
}

// ---------------------------------------------------------------------------
// OpticalSolver::thinLensRefract
// ---------------------------------------------------------------------------
QPointF OpticalSolver::thinLensRefract(QPointF dir, QPointF normal,
                                       const QPointF& hitPt, BaseComponent* lens)
{
    const double f = lens->properties.value("focalLength", 150.0).toDouble();
    if (std::abs(f) < 1.0)
        return dir;

    const QPointF fwd = QPointF(-normal.x(), -normal.y());
    const QPointF tng = QPointF(-fwd.y(), fwd.x());

    const QPointF lensCenter = lens->pos();
    const double h = dot2d(hitPt - lensCenter, tng);

    const double d_fwd = dot2d(dir, fwd);
    const double d_tng = dot2d(dir, tng);
    const double d_tng_new = d_tng - d_fwd * h / f;

    const QPointF newDir = fwd * d_fwd + tng * d_tng_new;
    return normalize2d(newDir);
}

// ---------------------------------------------------------------------------
// OpticalSolver::snellRefract
// Vectorized Snell's law.
// normal must oppose the incoming ray direction (dot(normal,dir) < 0).
// Returns zero vector on total internal reflection.
// ---------------------------------------------------------------------------
QPointF OpticalSolver::snellRefract(QPointF dir, QPointF normal, double n1, double n2)
{
    const double ratio = n1 / n2;
    const double cosI  = -dot2d(dir, normal);  // positive for incoming ray
    const double sinT2 = ratio * ratio * (1.0 - cosI * cosI);

    if (sinT2 > 1.0)
        return QPointF(0.0, 0.0);  // total internal reflection

    const double cosT = std::sqrt(1.0 - sinT2);
    const QPointF refracted = dir * ratio + normal * (ratio * cosI - cosT);
    return normalize2d(refracted);
}

// ---------------------------------------------------------------------------
// OpticalSolver::cauchyIndex
// Cauchy dispersion formula: n(λ) = A + B/λ²  (λ in nm)
// ---------------------------------------------------------------------------
double OpticalSolver::cauchyIndex(double lambda_nm, double A, double B)
{
    if (lambda_nm < 1.0) return A;
    return A + B / (lambda_nm * lambda_nm);
}

// ---------------------------------------------------------------------------
// OpticalSolver::slitPassesRay
// Returns true if the hitPt falls inside one of the slit gaps.
// The slit surface runs along the item's local Y axis at local X = 0.
// ---------------------------------------------------------------------------
bool OpticalSolver::slitPassesRay(BaseComponent* slit, const QPointF& hitPt)
{
    const double slitW   = slit->properties.value("slitWidth",       12.0).toDouble();
    const double slitSep = slit->properties.value("slitSeparation",  40.0).toDouble();
    const int    numSlit = std::clamp(slit->properties.value("numSlits", 1).toInt(), 1, 2);

    // Convert scene hit point to local coordinates.
    const QPointF localHit = slit->mapFromScene(hitPt);
    const double localY = localHit.y();

    QList<double> gapCentres;
    if (numSlit == 1) {
        gapCentres << 0.0;
    } else {
        gapCentres << -slitSep * 0.5 << slitSep * 0.5;
    }

    for (double gc : gapCentres) {
        if (localY >= gc - slitW * 0.5 && localY <= gc + slitW * 0.5)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// OpticalSolver::recordScreenHit
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
