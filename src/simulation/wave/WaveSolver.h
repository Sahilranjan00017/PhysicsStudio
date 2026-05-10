#pragma once

#include "simulation/SolverInterfaces.h"

#include <QList>
#include <vector>

class BaseComponent;

// ---------------------------------------------------------------------------
// WaveDomain
// Holds the persistent state for the 2-D wave simulation:
//   - sources / detectors — pointers into the scene (not owned)
//   - field              — flat (rows × cols) float array of amplitudes
//   - simTime            — accumulated simulation time in seconds
//
// The analytical model: each WaveSource emits a 2-D circular harmonic wave
//   ψ_i(x,y,t) = A_i · sin(ω_i·t − k_i·r + φ_i) · attenuation(r)
// The field is the superposition of all ψ_i and is recomputed every tick.
//
// WaveFieldOverlay reads field, cols, rows, gridSize to render the pattern.
// ---------------------------------------------------------------------------
struct WaveDomain {
    QList<BaseComponent*> sources;    // WaveSourceComponent objects
    QList<BaseComponent*> detectors;  // WaveDetectorComponent objects

    std::vector<float> field;         // amplitude grid, row-major, size cols×rows
    int    cols      = 200;           // grid width  (scene width  / gridSize)
    int    rows      = 134;           // grid height (scene height / gridSize)
    double gridSize  = 12.0;          // pixels per grid cell
    double simTime   = 0.0;           // accumulated time (seconds)
    double waveSpeed = 200.0;         // px / s — governs wavelength λ = v/f
};

// ---------------------------------------------------------------------------
// WaveSolver
// Analytical superposition solver — recomputes the full field every tick.
// O(sources × cols × rows) per step; fast enough for ≤10 sources at 60 FPS.
// ---------------------------------------------------------------------------
class WaveSolver final : public IWaveSolver {
public:
    void step(WaveDomain& domain, double dt) override;
};
