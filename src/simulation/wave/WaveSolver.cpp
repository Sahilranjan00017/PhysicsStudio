#include "simulation/wave/WaveSolver.h"

#include "components/BaseComponent.h"

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <numbers>

// ---------------------------------------------------------------------------
// Internal helper: stamp one circular harmonic source into the field.
// ---------------------------------------------------------------------------
namespace {

inline double dotPt(const QPointF& a, const QPointF& b)
{
    return a.x() * b.x() + a.y() * b.y();
}

void stampSource(std::vector<float>& field,
                 int cols, int rows, double gs,
                 double sx, double sy,
                 double freq, double amp, double phaseDeg,
                 double waveSpeed, double t)
{
    const double omega    = 2.0 * std::numbers::pi * freq;
    const double k        = omega / waveSpeed;
    const double phaseRad = phaseDeg * std::numbers::pi / 180.0;
    const double sinOmegaT = std::sin(omega * t + phaseRad);
    const double cosOmegaT = std::cos(omega * t + phaseRad);

    for (int iy = 0; iy < rows; ++iy) {
        const double cy  = iy * gs;
        const double dy  = cy - sy;
        const double dy2 = dy * dy;

        float* row = field.data() + iy * cols;

        for (int ix = 0; ix < cols; ++ix) {
            const double cx = ix * gs;
            const double dx = cx - sx;
            const double r  = std::sqrt(dx * dx + dy2);

            if (r < 0.5) continue;

            const double kr   = k * r;
            const double wave = amp * (sinOmegaT * std::cos(kr) - cosOmegaT * std::sin(kr));
            const double attn = 1.0 / std::sqrt(std::max(r / 30.0, 1.0));

            row[ix] += static_cast<float>(wave * attn);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// WaveSolver::step
// ---------------------------------------------------------------------------
void WaveSolver::step(WaveDomain& domain, double dt)
{
    const bool hasSources  = !domain.sources.isEmpty()  || !domain.planeSources.isEmpty();
    const bool hasBarriers = !domain.barriers.isEmpty()  || !domain.walls.isEmpty();

    if (!hasSources && !hasBarriers)
        return;

    domain.simTime += dt;

    const int    cols = domain.cols;
    const int    rows = domain.rows;
    const double gs   = domain.gridSize;
    const double v    = domain.waveSpeed;
    const double t    = domain.simTime;

    // Resize / clear the field.
    domain.field.assign(static_cast<size_t>(cols * rows), 0.0f);

    // ── Primary point sources (WAV_SRC, WAV_SOUND, WAV_RIPPLE) ──────────────
    for (auto* src : domain.sources) {
        const double freq     = src->properties.value("frequency", 2.0).toDouble();
        const double amp      = src->properties.value("amplitude", 1.0).toDouble();
        const double phaseDeg = src->properties.value("phase",     0.0).toDouble();

        // WAV_RIPPLE: apply finite-duration bell envelope sin(π·t/T).
        double effAmp = amp;
        if (src->typeId == QLatin1String("WAV_RIPPLE")) {
            const double numCycles = src->properties.value("numCycles", 3.0).toDouble();
            const double duration  = (freq > 0.0) ? (numCycles / freq) : 0.0;
            if (duration > 0.0) {
                effAmp = (t < duration) ? amp * std::sin(std::numbers::pi * t / duration) : 0.0;
            }
        }

        if (effAmp > 1e-4) {
            stampSource(domain.field, cols, rows, gs,
                        src->pos().x(), src->pos().y(),
                        freq, effAmp, phaseDeg, v, t);
        }

        src->simState["simTime"] = t;
        src->update();
    }

    // ── Plane wave sources (WAV_PLANE) ────────────────────────────────────
    // Each component stamps a row of N coherent point sources along its local Y axis.
    // Constructive interference in the +X direction produces a planar wavefront.
    for (auto* src : domain.planeSources) {
        const double freq     = src->properties.value("frequency", 2.0).toDouble();
        const double amp      = src->properties.value("amplitude", 1.0).toDouble();
        const double phaseDeg = src->properties.value("phase",     0.0).toDouble();
        const double width    = src->properties.value("width",   120.0).toDouble();
        const int    N        = std::max(2, static_cast<int>(width / 10.0));

        for (int i = 0; i < N; ++i) {
            const double localY = -width * 0.5 + i * width / (N - 1);
            const QPointF scene = src->mapToScene(QPointF(0.0, localY));
            stampSource(domain.field, cols, rows, gs,
                        scene.x(), scene.y(),
                        freq, amp, phaseDeg, v, t);
        }

        src->simState["simTime"] = t;
        src->update();
    }

    // ── Image sources for reflective walls (WAV_WALL) ─────────────────────
    // Method of images: mirror each primary source across the wall line with a
    // 180° phase shift (Dirichlet / hard-boundary condition: ψ = 0 at wall).
    for (auto* wall : domain.walls) {
        const double halfL      = wall->properties.value("length",     160.0).toDouble() * 0.5;
        const double reflectance= wall->properties.value("reflectance",  0.9).toDouble();

        // Wall runs along local Y axis (vertical by default).
        const QPointF p1 = wall->mapToScene(QPointF(0.0, -halfL));
        const QPointF p2 = wall->mapToScene(QPointF(0.0,  halfL));
        const QPointF wallVec = p2 - p1;
        const double  wallLen = std::sqrt(wallVec.x()*wallVec.x() + wallVec.y()*wallVec.y());
        if (wallLen < 1.0) continue;

        // Unit normal (perpendicular to wall; either side works symmetrically).
        const QPointF wallDir(wallVec.x() / wallLen, wallVec.y() / wallLen);
        const QPointF wallNorm(-wallDir.y(), wallDir.x());

        // Reflect all point sources (sources + plane-source sub-points).
        auto reflectSource = [&](double sx, double sy,
                                 double freq, double effAmp, double phaseDeg) {
            const QPointF toSrc(sx - p1.x(), sy - p1.y());
            const double distN = dotPt(toSrc, wallNorm);
            if (std::abs(distN) < 1.0) return; // source on wall — skip

            const double imgX = sx - 2.0 * distN * wallNorm.x();
            const double imgY = sy - 2.0 * distN * wallNorm.y();

            stampSource(domain.field, cols, rows, gs,
                        imgX, imgY,
                        freq, effAmp * reflectance,
                        phaseDeg + 180.0,   // hard boundary: 180° phase shift
                        v, t);
        };

        for (auto* src : domain.sources) {
            const double freq     = src->properties.value("frequency", 2.0).toDouble();
            const double amp      = src->properties.value("amplitude", 1.0).toDouble();
            const double phaseDeg = src->properties.value("phase",     0.0).toDouble();
            double effAmp = amp;
            if (src->typeId == QLatin1String("WAV_RIPPLE")) {
                const double nc  = src->properties.value("numCycles", 3.0).toDouble();
                const double dur = (freq > 0.0) ? (nc / freq) : 0.0;
                effAmp = (dur > 0.0 && t < dur)
                         ? amp * std::sin(std::numbers::pi * t / dur) : 0.0;
            }
            if (effAmp > 1e-4)
                reflectSource(src->pos().x(), src->pos().y(), freq, effAmp, phaseDeg);
        }
        for (auto* src : domain.planeSources) {
            const double freq     = src->properties.value("frequency", 2.0).toDouble();
            const double amp      = src->properties.value("amplitude", 1.0).toDouble();
            const double phaseDeg = src->properties.value("phase",     0.0).toDouble();
            const double width    = src->properties.value("width",   120.0).toDouble();
            const int    N        = std::max(2, static_cast<int>(width / 10.0));
            for (int i = 0; i < N; ++i) {
                const double localY = -width * 0.5 + i * width / (N - 1);
                const QPointF scene = src->mapToScene(QPointF(0.0, localY));
                reflectSource(scene.x(), scene.y(), freq, amp, phaseDeg);
            }
        }

        wall->simState["simTime"] = t;
        wall->update();
    }

    // ── Huygens secondary sources at barrier slits ────────────────────────
    for (auto* barrier : domain.barriers) {
        const double barrierLen = barrier->properties.value("barrierLength", 200.0).toDouble();
        const double slitW      = barrier->properties.value("slitWidth",      16.0).toDouble();
        const double slitSep    = barrier->properties.value("slitSeparation", 60.0).toDouble();
        const int    numSlits   = std::clamp(barrier->properties.value("numSlits", 1).toInt(), 1, 2);
        const double freq       = barrier->properties.value("frequency",       2.0).toDouble();
        const double amp        = barrier->properties.value("amplitude",       0.6).toDouble();

        QList<QPointF> slitCentres;
        if (numSlits == 1) {
            slitCentres << barrier->mapToScene(QPointF(0.0, 0.0));
        } else {
            slitCentres << barrier->mapToScene(QPointF(0.0, -slitSep * 0.5));
            slitCentres << barrier->mapToScene(QPointF(0.0,  slitSep * 0.5));
        }

        for (const QPointF& sc : slitCentres) {
            stampSource(domain.field, cols, rows, gs,
                        sc.x(), sc.y(), freq, amp, 0.0, v, t);
        }

        const QPointF barTop   = barrier->mapToScene(QPointF(0.0, -barrierLen * 0.5));
        const QPointF barBot   = barrier->mapToScene(QPointF(0.0,  barrierLen * 0.5));
        const QPointF barDelta = barBot - barTop;
        const double  barLen   = std::sqrt(barDelta.x()*barDelta.x() + barDelta.y()*barDelta.y());

        if (barLen > 0.5) {
            const int steps = static_cast<int>(barrierLen / gs) * 3;
            for (int s = 0; s <= steps; ++s) {
                const double along = s * barrierLen / steps - barrierLen * 0.5;
                bool inSlit = false;
                if (numSlits == 1) {
                    inSlit = std::abs(along) < slitW * 0.5;
                } else {
                    inSlit = std::abs(along + slitSep * 0.5) < slitW * 0.5
                          || std::abs(along - slitSep * 0.5) < slitW * 0.5;
                }
                if (!inSlit) {
                    const QPointF pt = barrier->mapToScene(QPointF(0.0, along));
                    const int ix = std::clamp(static_cast<int>(pt.x() / gs), 0, cols - 1);
                    const int iy = std::clamp(static_cast<int>(pt.y() / gs), 0, rows - 1);
                    domain.field[static_cast<size_t>(iy * cols + ix)] = 0.0f;
                }
            }
        }

        barrier->simState["simTime"] = t;
        barrier->update();
    }

    // ── Absorber regions (WAV_ABSORBER) ───────────────────────────────────
    // Multiply field cells within each absorber's radius by (1 − damp·falloff).
    for (auto* absorber : domain.absorbers) {
        const double cx     = absorber->pos().x();
        const double cy     = absorber->pos().y();
        const double radius = absorber->properties.value("radius",  60.0).toDouble();
        const double damp   = absorber->properties.value("damping",  0.95).toDouble();

        const int ixMin = std::max(0,      static_cast<int>((cx - radius) / gs));
        const int ixMax = std::min(cols-1, static_cast<int>((cx + radius) / gs) + 1);
        const int iyMin = std::max(0,      static_cast<int>((cy - radius) / gs));
        const int iyMax = std::min(rows-1, static_cast<int>((cy + radius) / gs) + 1);

        for (int iy = iyMin; iy <= iyMax; ++iy) {
            for (int ix = ixMin; ix <= ixMax; ++ix) {
                const double dx = ix * gs - cx;
                const double dy = iy * gs - cy;
                const double dist = std::sqrt(dx*dx + dy*dy);
                if (dist <= radius) {
                    const double falloff = 1.0 - dist / radius; // 1 at centre → 0 at edge
                    const float scale    = static_cast<float>(1.0 - damp * falloff);
                    domain.field[static_cast<size_t>(iy * cols + ix)] *= scale;
                }
            }
        }
    }

    // ── Sample field at detector positions ──────────────────────────────────
    for (auto* det : domain.detectors) {
        const int ix = std::clamp(static_cast<int>(det->pos().x() / gs), 0, cols - 1);
        const int iy = std::clamp(static_cast<int>(det->pos().y() / gs), 0, rows - 1);
        det->simState["amplitude"] = static_cast<double>(domain.field[iy * cols + ix]);
        det->update();
    }
}
