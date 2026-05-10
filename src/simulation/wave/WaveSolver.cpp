#include "simulation/wave/WaveSolver.h"

#include "components/BaseComponent.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// WaveSolver::step
// ---------------------------------------------------------------------------
void WaveSolver::step(WaveDomain& domain, double dt)
{
    if (domain.sources.isEmpty())
        return;

    domain.simTime += dt;

    const int    cols = domain.cols;
    const int    rows = domain.rows;
    const double gs   = domain.gridSize;
    const double v    = domain.waveSpeed;
    const double t    = domain.simTime;

    // Resize / clear the field.
    domain.field.assign(static_cast<size_t>(cols * rows), 0.0f);

    // Accumulate contribution of each source into the field.
    for (auto* src : domain.sources) {
        const double freq     = src->properties.value("frequency", 2.0).toDouble();
        const double amp      = src->properties.value("amplitude", 1.0).toDouble();
        const double phaseDeg = src->properties.value("phase",     0.0).toDouble();

        const double omega    = 2.0 * M_PI * freq;
        const double k        = omega / v;                         // rad / px
        const double phaseRad = phaseDeg * M_PI / 180.0;

        // Pre-compute the time-dependent terms (constant per row).
        const double sinOmegaT = std::sin(omega * t + phaseRad);
        const double cosOmegaT = std::cos(omega * t + phaseRad);

        const double sx = src->pos().x();
        const double sy = src->pos().y();

        for (int iy = 0; iy < rows; ++iy) {
            const double cy  = iy * gs;
            const double dy  = cy - sy;
            const double dy2 = dy * dy;

            float* row = domain.field.data() + iy * cols;

            for (int ix = 0; ix < cols; ++ix) {
                const double cx = ix * gs;
                const double dx = cx - sx;
                const double r  = std::sqrt(dx * dx + dy2);

                if (r < 0.5)
                    continue;   // avoid singularity at source position

                const double kr = k * r;

                // ψ = A · sin(ωt − kr + φ) · attenuation(r)
                // Using trig identity: sin(a−b) = sin(a)cos(b) − cos(a)sin(b)
                const double wave = amp
                                    * (sinOmegaT * std::cos(kr) - cosOmegaT * std::sin(kr));

                // Gentle 2-D spreading: amplitude ∝ 1/sqrt(r/r0), r0 = 30 px.
                const double attn = 1.0 / std::sqrt(std::max(r / 30.0, 1.0));

                row[ix] += static_cast<float>(wave * attn);
            }
        }

        // Write current simulation time to source's simState (for paint animation).
        src->simState["simTime"] = t;
        src->update();
    }

    // Sample the field at each detector's scene position.
    for (auto* det : domain.detectors) {
        const int ix = std::clamp(static_cast<int>(det->pos().x() / gs), 0, cols - 1);
        const int iy = std::clamp(static_cast<int>(det->pos().y() / gs), 0, rows - 1);
        det->simState["amplitude"] = static_cast<double>(domain.field[iy * cols + ix]);
        det->update();
    }
}
