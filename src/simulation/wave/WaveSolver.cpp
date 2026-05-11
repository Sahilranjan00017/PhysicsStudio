#include "simulation/wave/WaveSolver.h"

#include "components/BaseComponent.h"

#include <algorithm>
#include <cmath>
#include <numbers>

// ---------------------------------------------------------------------------
// Internal helper: stamp one circular harmonic source into the field.
// ---------------------------------------------------------------------------
namespace {

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
    const bool hasSources  = !domain.sources.isEmpty();
    const bool hasBarriers = !domain.barriers.isEmpty();

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

    // ── Primary sources (WAV_SRC, WAV_SOUND) ────────────────────────────────
    for (auto* src : domain.sources) {
        const double freq     = src->properties.value("frequency", 2.0).toDouble();
        const double amp      = src->properties.value("amplitude", 1.0).toDouble();
        const double phaseDeg = src->properties.value("phase",     0.0).toDouble();

        stampSource(domain.field, cols, rows, gs,
                    src->pos().x(), src->pos().y(),
                    freq, amp, phaseDeg, v, t);

        src->simState["simTime"] = t;
        src->update();
    }

    // ── Huygens secondary sources at barrier slits ────────────────────────
    // Each slit gap becomes a re-emitting point source at its centre,
    // modulating the incoming wave amplitude from the nearest primary source.
    for (auto* barrier : domain.barriers) {
        const double barrierLen = barrier->properties.value("barrierLength", 200.0).toDouble();
        const double slitW      = barrier->properties.value("slitWidth",      16.0).toDouble();
        const double slitSep    = barrier->properties.value("slitSeparation", 60.0).toDouble();
        const int    numSlits   = std::clamp(barrier->properties.value("numSlits", 1).toInt(), 1, 2);
        const double freq       = barrier->properties.value("frequency",       2.0).toDouble();
        const double amp        = barrier->properties.value("amplitude",       0.6).toDouble();

        // Build list of slit centres in scene space.
        QList<QPointF> slitCentres;
        if (numSlits == 1) {
            slitCentres << barrier->mapToScene(QPointF(0.0, 0.0));
        } else {
            slitCentres << barrier->mapToScene(QPointF(0.0, -slitSep * 0.5));
            slitCentres << barrier->mapToScene(QPointF(0.0,  slitSep * 0.5));
        }

        // Stamp each slit as a secondary point source.
        for (const QPointF& sc : slitCentres) {
            stampSource(domain.field, cols, rows, gs,
                        sc.x(), sc.y(),
                        freq, amp, 0.0, v, t);
        }

        // Block the field along the barrier (opaque regions between slits).
        // Find the barrier line in grid coordinates and zero those cells.
        // Barrier runs along the item's local Y axis (vertical bar by default).
        const QPointF barTop   = barrier->mapToScene(QPointF(0.0, -barrierLen * 0.5));
        const QPointF barBot   = barrier->mapToScene(QPointF(0.0,  barrierLen * 0.5));
        const QPointF barDelta = barBot - barTop;
        const double  barLen   = std::sqrt(barDelta.x()*barDelta.x() + barDelta.y()*barDelta.y());

        if (barLen > 0.5) {
            const int steps = static_cast<int>(barrierLen / gs) * 3;

            for (int s = 0; s <= steps; ++s) {
                const double along = s * barrierLen / steps - barrierLen * 0.5;
                // Check if this point is inside an opaque region.
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

    // ── Sample field at detector positions ──────────────────────────────────
    for (auto* det : domain.detectors) {
        const int ix = std::clamp(static_cast<int>(det->pos().x() / gs), 0, cols - 1);
        const int iy = std::clamp(static_cast<int>(det->pos().y() / gs), 0, rows - 1);
        det->simState["amplitude"] = static_cast<double>(domain.field[iy * cols + ix]);
        det->update();
    }
}
