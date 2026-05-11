#include "simulation/optics/OpticalSolver.h"

#include "components/BaseComponent.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

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

            traceRay(domain, srcPos, dir, wavelength, intensity, rayLength, 0, blockers,
                     1.0, UNPOLARISED);
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
                              double nMedium,
                              double polAngle)
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
    seg.start             = pos;
    seg.end               = hitComp ? hitPt : (pos + dir * remainingLength);
    seg.wavelength        = wavelength;
    seg.intensity         = intensity;
    seg.polarisationAngle = polAngle;
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
                 remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);

    // ── Lens ─────────────────────────────────────────────────────────────────
    } else if (hitType == "OPT_LENS") {
        const double transmittance = hitComp->properties.value("transmittance", 0.97).toDouble();
        const QPointF refracted = thinLensRefract(dir, hitNormal, hitPt, hitComp);
        traceRay(domain, hitPt, refracted, wavelength,
                 intensity * transmittance,
                 remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);

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
            n1 = 1.0;     // air → glass
            n2 = nGlass;
        } else {
            n1 = nMedium; // glass → air
            n2 = 1.0;
        }

        const QPointF refracted = snellRefract(dir, hitNormal, n1, n2);
        if (refracted.x() == 0.0 && refracted.y() == 0.0) {
            const QPointF reflected = normalize2d(dir - hitNormal * (2.0 * dot2d(dir, hitNormal)));
            traceRay(domain, hitPt, reflected, wavelength,
                     intensity * 0.95,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);
        } else {
            traceRay(domain, hitPt, refracted, wavelength,
                     intensity * 0.98,
                     remainingLength - consumed, bounceCount + 1, blockers, n2, polAngle);
        }

    // ── Filter — wavelength bandpass ─────────────────────────────────────
    } else if (hitType == "OPT_FILTER") {
        const double center = hitComp->properties.value("centerWavelength", 550.0).toDouble();
        const double bw     = hitComp->properties.value("bandwidth",         80.0).toDouble();
        const double trans  = hitComp->properties.value("transmittance",      0.90).toDouble();

        if (std::abs(wavelength - center) <= bw * 0.5) {
            traceRay(domain, hitPt, normalize2d(dir), wavelength,
                     intensity * trans,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);
        }

    // ── Slit — aperture blocker ───────────────────────────────────────────
    } else if (hitType == "OPT_SLIT") {
        if (slitPassesRay(hitComp, hitPt)) {
            traceRay(domain, hitPt, normalize2d(dir), wavelength,
                     intensity,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);
        }

    // ── Curved mirror — paraxial reflection ──────────────────────────────
    } else if (hitType == "OPT_CONCAVE") {
        const double reflectivity = hitComp->properties.value("reflectivity", 0.95).toDouble();
        const QPointF reflected   = curvedMirrorReflect(dir, hitNormal, hitPt, hitComp);
        traceRay(domain, hitPt, reflected, wavelength,
                 intensity * reflectivity,
                 remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);

    // ── Diffraction grating — multi-order transmission ────────────────────
    } else if (hitType == "OPT_GRATING") {
        const double d_nm       = std::max(hitComp->properties.value("gratingSpacing", 500.0).toDouble(), 1.0);
        const int    numOrders  = std::clamp(hitComp->properties.value("numOrders",  2).toInt(), 1, 3);
        const double trans      = hitComp->properties.value("transmittance", 0.90).toDouble();

        // Forward normal (in direction of ray travel through the grating).
        const QPointF fwdNormal = QPointF(-hitNormal.x(), -hitNormal.y());
        // Surface tangent (along the grating rulings).
        const QPointF tng       = QPointF(-hitNormal.y(), hitNormal.x());

        // Incoming tangential component = sin(θ_i).
        const double sinThetaI = dot2d(dir, tng);

        // Intensity per order: m=0 gets 40%, m=±1 each 25%, m=±2 each 5%.
        constexpr double ORDER_WEIGHT[] = { 0.40, 0.25, 0.05 };

        for (int m = -numOrders; m <= numOrders; ++m) {
            const double sinThetaM = sinThetaI + m * (wavelength / d_nm);
            if (std::abs(sinThetaM) >= 1.0) continue;  // evanescent

            const double cosThetaM = std::sqrt(1.0 - sinThetaM * sinThetaM);
            const QPointF newDir   = normalize2d(tng * sinThetaM + fwdNormal * cosThetaM);

            const int    absM   = std::min(std::abs(m), 2);
            const double weight = ORDER_WEIGHT[absM];

            traceRay(domain, hitPt, newDir, wavelength,
                     intensity * trans * weight,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);
        }

    // ── Beam splitter — simultaneous reflection + transmission ────────────
    } else if (hitType == "OPT_SPLITTER") {
        const double R = std::clamp(hitComp->properties.value("reflectance", 0.5).toDouble(), 0.0, 1.0);
        const double T = 1.0 - R;

        // Reflected branch.
        if (R * intensity >= 0.01) {
            const QPointF reflected = normalize2d(dir - hitNormal * (2.0 * dot2d(dir, hitNormal)));
            traceRay(domain, hitPt, reflected, wavelength,
                     intensity * R,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);
        }
        // Transmitted branch (continues forward).
        if (T * intensity >= 0.01) {
            traceRay(domain, hitPt, normalize2d(dir), wavelength,
                     intensity * T,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium, polAngle);
        }

    // ── Polariser — Malus's law ───────────────────────────────────────────
    } else if (hitType == "OPT_POLARISER") {
        const double polAxis = hitComp->properties.value("angle", 0.0).toDouble()
                               * std::numbers::pi / 180.0;
        const double trans   = hitComp->properties.value("transmittance", 1.0).toDouble();

        double outIntensity;
        if (std::isnan(polAngle)) {
            // Unpolarised input: I_out = I_in / 2 (Malus's law, averaged over all angles).
            outIntensity = intensity * trans * 0.5;
        } else {
            // Already polarised: Malus's law I_out = I_in · cos²(Δθ).
            const double cosTheta = std::cos(polAngle - polAxis);
            outIntensity = intensity * trans * cosTheta * cosTheta;
        }

        if (outIntensity >= 0.01) {
            traceRay(domain, hitPt, normalize2d(dir), wavelength,
                     outIntensity,
                     remainingLength - consumed, bounceCount + 1, blockers, nMedium,
                     polAxis);   // outgoing ray is linearly polarised at polAxis
        }
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
    // ── New elements ────────────────────────────────────────────────────────
    if (comp->typeId == "OPT_CONCAVE" || comp->typeId == "OPT_SPLITTER") {
        // Surface runs along local X axis (like OPT_MIRROR).
        const double half = comp->properties.value("length", 120.0).toDouble() * 0.5;
        outA = comp->mapToScene(QPointF(-half, 0.0));
        outB = comp->mapToScene(QPointF( half, 0.0));
        return true;
    }
    if (comp->typeId == "OPT_GRATING" || comp->typeId == "OPT_POLARISER") {
        // Surface runs along local Y axis (like OPT_FILTER).
        const double half = comp->properties.value("length", 100.0).toDouble() * 0.5;
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
// OpticalSolver::curvedMirrorReflect
// Paraxial spherical-mirror model.
// At hit height h from the mirror centre along the surface, the effective
// surface normal is tilted by h/(2f) from the vertex normal (paraxial appr.).
// f > 0 = concave (converging);  f < 0 = convex (diverging).
// ---------------------------------------------------------------------------
QPointF OpticalSolver::curvedMirrorReflect(QPointF dir, QPointF flatNormal,
                                            const QPointF& hitPt, BaseComponent* mirror)
{
    const double f = mirror->properties.value("focalLength", 200.0).toDouble();
    if (std::abs(f) < 1.0) {
        // Degenerate — fall back to flat mirror.
        return normalize2d(dir - flatNormal * (2.0 * dot2d(dir, flatNormal)));
    }

    // Surface tangent (along the mirror surface, perpendicular to normal).
    const QPointF tng(-flatNormal.y(), flatNormal.x());

    // Height from mirror vertex along the surface tangent.
    const double h = dot2d(hitPt - mirror->pos(), tng);

    // Paraxial effective-normal tilt: dθ = h / (2f).
    const double tilt = h / (2.0 * f);
    const QPointF effectiveNormal = normalize2d(flatNormal - tng * tilt);

    // Standard specular reflection off the effective normal.
    return normalize2d(dir - effectiveNormal * (2.0 * dot2d(dir, effectiveNormal)));
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
