#include "simulation/wave/WaveSolver.h"

#include "components/BaseComponent.h"

#include <QPointF>

#include <algorithm>
#include <cmath>
#include <numbers>

// ---------------------------------------------------------------------------
// Internal helper: stamp one circular harmonic source into the field.
//
// All inner-loop arithmetic is done in float (not double) for speed:
//   • float sin/cos/sqrt are typically 2–4× faster than their double equivalents
//   • The field array is float anyway, so no precision is lost in the result
//   • Wave visuals need ~2 decimal places of accuracy — float gives 7
// ---------------------------------------------------------------------------
namespace {

inline double dotPt(const QPointF& a, const QPointF& b)
{
    return a.x() * b.x() + a.y() * b.y();
}

void stampSource(std::vector<float>& field,
                 int cols, int rows, float gs,
                 float sx, float sy,
                 float freq, float amp, float phaseDeg,
                 float waveSpeed, float t)
{
    constexpr float kPi   = static_cast<float>(std::numbers::pi);
    const float omega     = 2.0f * kPi * freq;
    const float k         = omega / waveSpeed;
    const float phaseRad  = phaseDeg * kPi / 180.0f;
    // Pre-compute time-dependent terms once (same for every grid cell).
    const float sinOT     = std::sin(omega * t + phaseRad);
    const float cosOT     = std::cos(omega * t + phaseRad);

    for (int iy = 0; iy < rows; ++iy) {
        const float cy  = static_cast<float>(iy) * gs;
        const float dy  = cy - sy;
        const float dy2 = dy * dy;

        float* row = field.data() + iy * cols;

        for (int ix = 0; ix < cols; ++ix) {
            const float cx = static_cast<float>(ix) * gs;
            const float dx = cx - sx;
            const float r  = std::sqrt(dx * dx + dy2);

            if (r < 0.5f) continue;

            const float kr   = k * r;
            // Trig identity: sin(ωt − kr) = sin(ωt)cos(kr) − cos(ωt)sin(kr)
            const float wave = amp * (sinOT * std::cos(kr) - cosOT * std::sin(kr));
            // 1/√(r/30) attenuation: full amplitude up to 30 px, ∝ 1/√r beyond.
            const float attn = 1.0f / std::sqrt(std::max(r / 30.0f, 1.0f));

            row[ix] += wave * attn;
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

    const int   cols = domain.cols;
    const int   rows = domain.rows;
    const float gs   = static_cast<float>(domain.gridSize);
    const float v    = static_cast<float>(domain.waveSpeed);
    const float t    = static_cast<float>(domain.simTime);

    // Resize / clear the field.
    domain.field.assign(static_cast<size_t>(cols * rows), 0.0f);

    // ── Primary point sources (WAV_SRC, WAV_SOUND, WAV_RIPPLE) ──────────────
    for (auto* src : domain.sources) {
        const float freq     = static_cast<float>(src->properties.value("frequency", 2.0).toDouble());
        const float amp      = static_cast<float>(src->properties.value("amplitude", 1.0).toDouble());
        const float phaseDeg = static_cast<float>(src->properties.value("phase",     0.0).toDouble());

        // WAV_RIPPLE: apply finite-duration bell envelope sin(π·t/T).
        float effAmp = amp;
        if (src->typeId == QLatin1String("WAV_RIPPLE")) {
            const float numCycles = static_cast<float>(src->properties.value("numCycles", 3.0).toDouble());
            const float duration  = (freq > 0.0f) ? (numCycles / freq) : 0.0f;
            if (duration > 0.0f) {
                constexpr float kPi = static_cast<float>(std::numbers::pi);
                effAmp = (t < duration) ? amp * std::sin(kPi * t / duration) : 0.0f;
            }
        }

        if (effAmp > 1e-4f) {
            stampSource(domain.field, cols, rows, gs,
                        static_cast<float>(src->pos().x()),
                        static_cast<float>(src->pos().y()),
                        freq, effAmp, phaseDeg, v, t);
        }

        src->simState["simTime"] = static_cast<double>(t);
        QMetaObject::invokeMethod(src, [src]() { src->update(); }, Qt::QueuedConnection);
    }

    // ── Plane wave sources (WAV_PLANE) ────────────────────────────────────
    // Each component stamps a row of N coherent point sources along its local Y axis.
    // Constructive interference in the +X direction produces a planar wavefront.
    for (auto* src : domain.planeSources) {
        const float freq     = static_cast<float>(src->properties.value("frequency", 2.0).toDouble());
        const float amp      = static_cast<float>(src->properties.value("amplitude", 1.0).toDouble());
        const float phaseDeg = static_cast<float>(src->properties.value("phase",     0.0).toDouble());
        const float width    = static_cast<float>(src->properties.value("width",   120.0).toDouble());
        const int   N        = std::max(2, static_cast<int>(width / 10.0f));

        for (int i = 0; i < N; ++i) {
            const float localY = -width * 0.5f + static_cast<float>(i) * width / static_cast<float>(N - 1);
            const QPointF scene = src->mapToScene(QPointF(static_cast<double>(0.0f), static_cast<double>(localY)));
            stampSource(domain.field, cols, rows, gs,
                        static_cast<float>(scene.x()),
                        static_cast<float>(scene.y()),
                        freq, amp, phaseDeg, v, t);
        }

        src->simState["simTime"] = static_cast<double>(t);
        QMetaObject::invokeMethod(src, [src]() { src->update(); }, Qt::QueuedConnection);
    }

    // ── Image sources for reflective walls (WAV_WALL) ─────────────────────
    // Method of images: mirror each primary source across the wall line with a
    // 180° phase shift (Dirichlet / hard-boundary condition: ψ = 0 at wall).
    for (auto* wall : domain.walls) {
        const float halfL      = static_cast<float>(wall->properties.value("length",     160.0).toDouble()) * 0.5f;
        const float reflectance= static_cast<float>(wall->properties.value("reflectance",  0.9).toDouble());

        // Wall runs along local Y axis (vertical by default).
        const QPointF p1 = wall->mapToScene(QPointF(0.0, static_cast<double>(-halfL)));
        const QPointF p2 = wall->mapToScene(QPointF(0.0, static_cast<double>( halfL)));
        const QPointF wallVec = p2 - p1;
        const double  wallLen = std::sqrt(wallVec.x()*wallVec.x() + wallVec.y()*wallVec.y());
        if (wallLen < 1.0) continue;

        // Unit normal (perpendicular to wall; either side works symmetrically).
        const QPointF wallDir(wallVec.x() / wallLen, wallVec.y() / wallLen);
        const QPointF wallNorm(-wallDir.y(), wallDir.x());

        // Reflect all point sources (sources + plane-source sub-points).
        auto reflectSource = [&](float sx, float sy,
                                 float freq, float effAmp, float phaseDeg) {
            const QPointF toSrc(static_cast<double>(sx) - p1.x(),
                                static_cast<double>(sy) - p1.y());
            const double distN = dotPt(toSrc, wallNorm);
            if (std::abs(distN) < 1.0) return; // source on wall — skip

            const float imgX = sx - 2.0f * static_cast<float>(distN) * static_cast<float>(wallNorm.x());
            const float imgY = sy - 2.0f * static_cast<float>(distN) * static_cast<float>(wallNorm.y());

            stampSource(domain.field, cols, rows, gs,
                        imgX, imgY,
                        freq, effAmp * reflectance,
                        phaseDeg + 180.0f,   // hard boundary: 180° phase shift
                        v, t);
        };

        for (auto* src : domain.sources) {
            const float freq     = static_cast<float>(src->properties.value("frequency", 2.0).toDouble());
            const float amp      = static_cast<float>(src->properties.value("amplitude", 1.0).toDouble());
            const float phaseDeg = static_cast<float>(src->properties.value("phase",     0.0).toDouble());
            float effAmp = amp;
            if (src->typeId == QLatin1String("WAV_RIPPLE")) {
                const float nc  = static_cast<float>(src->properties.value("numCycles", 3.0).toDouble());
                const float dur = (freq > 0.0f) ? (nc / freq) : 0.0f;
                constexpr float kPi = static_cast<float>(std::numbers::pi);
                effAmp = (dur > 0.0f && t < dur)
                         ? amp * std::sin(kPi * t / dur) : 0.0f;
            }
            if (effAmp > 1e-4f)
                reflectSource(static_cast<float>(src->pos().x()),
                              static_cast<float>(src->pos().y()),
                              freq, effAmp, phaseDeg);
        }
        for (auto* src : domain.planeSources) {
            const float freq     = static_cast<float>(src->properties.value("frequency", 2.0).toDouble());
            const float amp      = static_cast<float>(src->properties.value("amplitude", 1.0).toDouble());
            const float phaseDeg = static_cast<float>(src->properties.value("phase",     0.0).toDouble());
            const float width    = static_cast<float>(src->properties.value("width",   120.0).toDouble());
            const int   N        = std::max(2, static_cast<int>(width / 10.0f));
            for (int i = 0; i < N; ++i) {
                const float localY = -width * 0.5f + static_cast<float>(i) * width / static_cast<float>(N - 1);
                const QPointF scene = src->mapToScene(QPointF(0.0, static_cast<double>(localY)));
                reflectSource(static_cast<float>(scene.x()),
                              static_cast<float>(scene.y()),
                              freq, amp, phaseDeg);
            }
        }

        wall->simState["simTime"] = static_cast<double>(t);
        QMetaObject::invokeMethod(wall, [wall]() { wall->update(); }, Qt::QueuedConnection);
    }

    // ── Huygens secondary sources at barrier slits ────────────────────────
    for (auto* barrier : domain.barriers) {
        const float barrierLen = static_cast<float>(barrier->properties.value("barrierLength", 200.0).toDouble());
        const float slitW      = static_cast<float>(barrier->properties.value("slitWidth",      16.0).toDouble());
        const float slitSep    = static_cast<float>(barrier->properties.value("slitSeparation", 60.0).toDouble());
        const int   numSlits   = std::clamp(barrier->properties.value("numSlits", 1).toInt(), 1, 2);
        const float freq       = static_cast<float>(barrier->properties.value("frequency",       2.0).toDouble());
        const float amp        = static_cast<float>(barrier->properties.value("amplitude",       0.6).toDouble());

        QList<QPointF> slitCentres;
        if (numSlits == 1) {
            slitCentres << barrier->mapToScene(QPointF(0.0, 0.0));
        } else {
            slitCentres << barrier->mapToScene(QPointF(0.0, static_cast<double>(-slitSep * 0.5f)));
            slitCentres << barrier->mapToScene(QPointF(0.0, static_cast<double>( slitSep * 0.5f)));
        }

        for (const QPointF& sc : slitCentres) {
            stampSource(domain.field, cols, rows, gs,
                        static_cast<float>(sc.x()),
                        static_cast<float>(sc.y()),
                        freq, amp, 0.0f, v, t);
        }

        const QPointF barTop   = barrier->mapToScene(QPointF(0.0, static_cast<double>(-barrierLen * 0.5f)));
        const QPointF barBot   = barrier->mapToScene(QPointF(0.0, static_cast<double>( barrierLen * 0.5f)));
        const QPointF barDelta = barBot - barTop;
        const double  barLen   = std::sqrt(barDelta.x()*barDelta.x() + barDelta.y()*barDelta.y());

        if (barLen > 0.5) {
            const int steps = static_cast<int>(barrierLen / gs) * 3;
            for (int s = 0; s <= steps; ++s) {
                const float along = static_cast<float>(s) * barrierLen / static_cast<float>(steps) - barrierLen * 0.5f;
                bool inSlit = false;
                if (numSlits == 1) {
                    inSlit = std::abs(along) < slitW * 0.5f;
                } else {
                    inSlit = std::abs(along + slitSep * 0.5f) < slitW * 0.5f
                          || std::abs(along - slitSep * 0.5f) < slitW * 0.5f;
                }
                if (!inSlit) {
                    const QPointF pt = barrier->mapToScene(QPointF(0.0, static_cast<double>(along)));
                    const int ix = std::clamp(static_cast<int>(static_cast<float>(pt.x()) / gs), 0, cols - 1);
                    const int iy = std::clamp(static_cast<int>(static_cast<float>(pt.y()) / gs), 0, rows - 1);
                    domain.field[static_cast<size_t>(iy * cols + ix)] = 0.0f;
                }
            }
        }

        barrier->simState["simTime"] = static_cast<double>(t);
        QMetaObject::invokeMethod(barrier, [barrier]() { barrier->update(); }, Qt::QueuedConnection);
    }

    // ── Absorber regions (WAV_ABSORBER) ───────────────────────────────────
    // Multiply field cells within each absorber's radius by (1 − damp·falloff).
    for (auto* absorber : domain.absorbers) {
        const float cx     = static_cast<float>(absorber->pos().x());
        const float cy     = static_cast<float>(absorber->pos().y());
        const float radius = static_cast<float>(absorber->properties.value("radius",  60.0).toDouble());
        const float damp   = static_cast<float>(absorber->properties.value("damping",  0.95).toDouble());

        const int ixMin = std::max(0,      static_cast<int>((cx - radius) / gs));
        const int ixMax = std::min(cols-1, static_cast<int>((cx + radius) / gs) + 1);
        const int iyMin = std::max(0,      static_cast<int>((cy - radius) / gs));
        const int iyMax = std::min(rows-1, static_cast<int>((cy + radius) / gs) + 1);

        for (int iy = iyMin; iy <= iyMax; ++iy) {
            for (int ix = ixMin; ix <= ixMax; ++ix) {
                const float dx   = static_cast<float>(ix) * gs - cx;
                const float dy   = static_cast<float>(iy) * gs - cy;
                const float dist = std::sqrt(dx*dx + dy*dy);
                if (dist <= radius) {
                    const float falloff = 1.0f - dist / radius;  // 1 at centre → 0 at edge
                    const float scale   = 1.0f - damp * falloff;
                    domain.field[static_cast<size_t>(iy * cols + ix)] *= scale;
                }
            }
        }
    }

    // ── Sample field at detector positions ──────────────────────────────────
    for (auto* det : domain.detectors) {
        const int ix = std::clamp(static_cast<int>(static_cast<float>(det->pos().x()) / gs), 0, cols - 1);
        const int iy = std::clamp(static_cast<int>(static_cast<float>(det->pos().y()) / gs), 0, rows - 1);
        det->simState["amplitude"] = static_cast<double>(domain.field[iy * cols + ix]);
        QMetaObject::invokeMethod(det, [det]() { det->update(); }, Qt::QueuedConnection);
    }
}
