#include "simulation/electronics/ElectronicsSolver.h"

#include "components/BaseComponent.h"
#include "components/ConnectionPad.h"
#include "components/Wire.h"

#include <Eigen/Dense>

#include <QDebug>
#include <QMap>

#include <cmath>
#include <numbers>

// ---------------------------------------------------------------------------
// MNA solver — Modified Nodal Analysis
//
// Matrix equation:  G · x = b
//
//   G is (n + m) × (n + m) where
//     n = number of nodes (excluding ground node 0)
//     m = number of independent voltage sources
//
//   x = [ v_1 … v_n | i_s1 … i_sm ]^T   (unknowns)
//   b = [ i_1 … i_n | e_1  … e_m  ]^T   (RHS)
//
// Component stamps:
//   Resistor (a→b, R):
//     G[a][a] += 1/R        G[a][b] -= 1/R
//     G[b][a] -= 1/R        G[b][b] += 1/R
//
//   Voltage source (a→b, E):
//     G[a][n+k] += 1        G[n+k][a] += 1
//     G[b][n+k] -= 1        G[n+k][b] -= 1
//     b[n+k]     = E
//
//   Current source (a→b, I):
//     b[a] -= I             b[b] += I
//
//   Ammeter / Voltmeter are stamped as near-zero resistance shunts so
//   they don't require extra unknowns (avoids over-constrained matrices in
//   simple tutorial circuits).
// ---------------------------------------------------------------------------

namespace {

// Ground node is index -1; all other pads get sequential indices 0..n-1.
using NodeMap = QMap<ConnectionPad*, int>;

// Walk the wire graph and assign node indices.
// Pads connected by a wire share the same node index.
NodeMap buildNodeMap(const ElectronicsDomain& domain, int& outNodeCount)
{
    NodeMap map;

    // Assign each electrical pad a preliminary unique id.
    int nextId = 0;
    for (auto* comp : domain.components) {
        for (auto* pad : comp->pads) {
            if (pad->domain == DomainType::Electrical && !map.contains(pad)) {
                map[pad] = nextId++;
            }
        }
    }

    // Merge ids for pads joined by wires (union-find via direct relabelling).
    for (auto* wire : domain.wires) {
        if (!wire->startPad || !wire->endPad) continue;
        if (!map.contains(wire->startPad) || !map.contains(wire->endPad)) continue;

        int keep    = map[wire->startPad];
        int replace = map[wire->endPad];
        if (keep == replace) continue;

        // relabel every pad that had replace → keep
        for (auto it = map.begin(); it != map.end(); ++it) {
            if (it.value() == replace)
                it.value() = keep;
        }
    }

    // Compact indices 0..n-1 (some may have been merged away).
    QMap<int, int> compact;
    int next = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (!compact.contains(it.value()))
            compact[it.value()] = next++;
        it.value() = compact[it.value()];
    }

    outNodeCount = next;
    return map;
}

// Returns the node index for this pad, or -1 for ground.
int nodeOf(ConnectionPad* pad, const NodeMap& map)
{
    if (!pad) return -1;
    auto it = map.find(pad);
    return it != map.end() ? it.value() : -1;
}

// Find the ground node index: look for a GroundComponent pad.
int findGroundNode(const ElectronicsDomain& domain, const NodeMap& map)
{
    for (auto* comp : domain.components) {
        if (comp->typeId == "ELEC_GND") {
            for (auto* pad : comp->pads) {
                if (map.contains(pad))
                    return map[pad];
            }
        }
    }
    return -1; // not found
}

} // namespace

// ---------------------------------------------------------------------------

bool ElectronicsSolver::validate(const ElectronicsDomain& domain) const
{
    if (domain.components.isEmpty()) return false;
    bool hasGround = false;
    for (auto* c : domain.components)
        if (c->typeId == "ELEC_GND") { hasGround = true; break; }
    if (!hasGround) {
        qWarning() << "ElectronicsSolver: no ground node in circuit — skipping";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------

void ElectronicsSolver::solve(ElectronicsDomain& domain, double dt)
{
    if (!validate(domain)) return;

    // Advance simulation time (used by AC source).
    domain.simTime += dt;

    // -----------------------------------------------------------------------
    // Step 1 — build node map
    // -----------------------------------------------------------------------
    int n = 0;
    NodeMap nodeMap = buildNodeMap(domain, n);

    int groundNode = findGroundNode(domain, nodeMap);
    if (groundNode < 0) return; // already warned

    // -----------------------------------------------------------------------
    // Step 2 — count voltage sources (VSRC + AC need extra MNA unknowns)
    // -----------------------------------------------------------------------
    int m = 0;
    QList<BaseComponent*> vsources;
    for (auto* comp : domain.components) {
        if (comp->typeId == "ELEC_VSRC" || comp->typeId == "ELEC_AC") {
            vsources.append(comp);
            ++m;
        }
    }

    // System size: n nodes + m voltage-source currents.
    // We will eliminate the ground row/column so size = (n-1) + m.
    const int size = (n - 1) + m;
    if (size <= 0) return;

    // Map from original node index → compressed index (skipping ground).
    // groundNode → -1, others 0..n-2.
    auto compress = [&](int node) -> int {
        if (node < 0 || node == groundNode) return -1;
        return node < groundNode ? node : node - 1;
    };

    Eigen::MatrixXd G = Eigen::MatrixXd::Zero(size, size);
    Eigen::VectorXd b = Eigen::VectorXd::Zero(size);

    // -----------------------------------------------------------------------
    // Step 3 — stamp all passive / controlled elements
    // -----------------------------------------------------------------------
    for (auto* comp : domain.components) {
        const QString& type = comp->typeId;

        // ── Resistive elements (Resistor, Ammeter, Voltmeter, Switch, Fuse) ──
        if (type == "ELEC_RES" || type == "ELEC_AMM" || type == "ELEC_VOLTM"
         || type == "ELEC_SW"  || type == "ELEC_FUSE") {
            double R = 1.0;
            if (type == "ELEC_RES")
                R = comp->properties.value("resistance", 1000.0).toDouble();
            else if (type == "ELEC_AMM")
                R = comp->properties.value("burdenResistance", 1e-6).toDouble();
            else if (type == "ELEC_VOLTM")
                R = comp->properties.value("inputResistance", 1e9).toDouble();
            else if (type == "ELEC_SW") {
                const bool closed = comp->properties.value("closed", true).toBool();
                R = closed ? 0.001 : 1e9;
            } else { // ELEC_FUSE
                R = comp->destroyed ? 1e9
                    : comp->properties.value("resistance", 0.01).toDouble();
            }

            if (R <= 0.0) R = 1e-9;
            const double G_val = 1.0 / R;

            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

            if (a >= 0)     G(a,     a    ) += G_val;
            if (b_idx >= 0) G(b_idx, b_idx) += G_val;
            if (a >= 0 && b_idx >= 0) {
                G(a,     b_idx) -= G_val;
                G(b_idx, a    ) -= G_val;
            }
        }
        // ── Capacitor companion model (backward-Euler: G_eq || I_hist) ──
        else if (type == "ELEC_CAP") {
            const double C     = comp->properties.value("capacitance", 100e-6).toDouble();
            const double G_eq  = (dt > 1e-12) ? C / dt : 0.0;
            const double V_prev = comp->simState.value("v_cap", 0.0).toDouble();

            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

            // Equivalent conductance stamp.
            if (a >= 0)     G(a,     a    ) += G_eq;
            if (b_idx >= 0) G(b_idx, b_idx) += G_eq;
            if (a >= 0 && b_idx >= 0) {
                G(a,     b_idx) -= G_eq;
                G(b_idx, a    ) -= G_eq;
            }
            // History current source (b→a direction, magnitude G_eq*V_prev).
            const double I_hist = G_eq * V_prev;
            if (a >= 0)     b(a)     += I_hist;
            if (b_idx >= 0) b(b_idx) -= I_hist;
        }
        // ── Diode / LED  (piecewise-linear, no extra MNA unknown) ──
        else if (type == "ELEC_DIODE" || type == "ELEC_LED") {
            const double Vf   = comp->properties.value("forwardVoltage",
                                    (type == "ELEC_LED") ? 2.0 : 0.7).toDouble();
            const double Ron  = comp->properties.value("onResistance",
                                    (type == "ELEC_LED") ? 5.0 : 0.1).toDouble();
            constexpr double Roff = 1e8;

            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

            // Use previous voltage to determine bias state.
            const double V_prev = comp->simState.value("voltageDiff", 0.0).toDouble();
            const bool fwd = (V_prev > Vf * 0.5);

            const double G_val = 1.0 / (fwd ? Ron : Roff);

            if (a >= 0)     G(a,     a    ) += G_val;
            if (b_idx >= 0) G(b_idx, b_idx) += G_val;
            if (a >= 0 && b_idx >= 0) {
                G(a,     b_idx) -= G_val;
                G(b_idx, a    ) -= G_val;
            }
            // Forward voltage offset: current source G*Vf injected into node a.
            if (fwd) {
                const double Ioffset = G_val * Vf;
                if (a >= 0)     b(a)     += Ioffset;
                if (b_idx >= 0) b(b_idx) -= Ioffset;
            }
        }
        // ── Current source ──
        else if (type == "ELEC_ISRC") {
            const double I = comp->properties.value("current", 0.01).toDouble();
            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));
            if (a >= 0)     b(a)     -= I;
            if (b_idx >= 0) b(b_idx) += I;
        }
        // Ground and voltage sources / AC handled separately below.
    }

    // -----------------------------------------------------------------------
    // Step 4 — stamp voltage sources (VSRC) and AC sources (ELEC_AC)
    // -----------------------------------------------------------------------
    for (int k = 0; k < vsources.size(); ++k) {
        auto* comp  = vsources[k];
        double E;
        if (comp->typeId == "ELEC_AC") {
            const double Vamp = comp->properties.value("voltage",    5.0).toDouble();
            const double freq = comp->properties.value("frequency", 50.0).toDouble();
            E = Vamp * std::sin(2.0 * std::numbers::pi * freq * domain.simTime);
        } else {
            E = comp->properties.value("voltage", 5.0).toDouble();
        }
        if (comp->pads.size() < 2) continue;

        // pad[0] = + terminal, pad[1] = - terminal (or ground side)
        const int a     = compress(nodeOf(comp->pads[0], nodeMap));
        const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));
        const int row   = (n - 1) + k;  // extra row for this source

        if (a >= 0) {
            G(a,   row) += 1.0;
            G(row, a  ) += 1.0;
        }
        if (b_idx >= 0) {
            G(b_idx, row) -= 1.0;
            G(row, b_idx) -= 1.0;
        }
        b(row) = E;
    }

    // -----------------------------------------------------------------------
    // Step 5 — solve G · x = b
    // -----------------------------------------------------------------------
    Eigen::VectorXd x;
    const Eigen::FullPivLU<Eigen::MatrixXd> lu(G);
    if (!lu.isInvertible()) {
        qWarning() << "ElectronicsSolver: singular matrix — circuit may be"
                   << "disconnected or have conflicting voltage sources";
        return;
    }
    x = lu.solve(b);

    // -----------------------------------------------------------------------
    // Step 6 — expand solution back to full node voltages (ground = 0)
    // -----------------------------------------------------------------------
    Eigen::VectorXd V = Eigen::VectorXd::Zero(n);
    for (int i = 0; i < n; ++i) {
        const int ci = compress(i);
        if (ci >= 0 && ci < (n - 1))
            V(i) = x(ci);
        // ground node stays 0
    }

    // -----------------------------------------------------------------------
    // Step 7 — write results back into component simState
    // -----------------------------------------------------------------------
    for (auto* comp : domain.components) {
        if (comp->pads.size() < 1) continue;

        const int nodeA = nodeOf(comp->pads[0], nodeMap);
        const int nodeB = comp->pads.size() >= 2
                          ? nodeOf(comp->pads[1], nodeMap) : -1;

        const double vA = (nodeA >= 0 && nodeA < n) ? V(nodeA) : 0.0;
        const double vB = (nodeB >= 0 && nodeB < n) ? V(nodeB) : 0.0;
        const double vDiff = vA - vB;

        comp->simState["voltageA"]   = vA;
        comp->simState["voltageB"]   = vB;
        comp->simState["voltageDiff"] = vDiff;

        const QString& type = comp->typeId;

        // ── Resistor / Ammeter ──
        if (type == "ELEC_RES" || type == "ELEC_AMM") {
            const double R = (type == "ELEC_RES")
                ? comp->properties.value("resistance",      1000.0).toDouble()
                : comp->properties.value("burdenResistance",   1e-6).toDouble();
            comp->simState["current"] = (R > 0.0) ? vDiff / R : 0.0;
        }
        // ── Voltmeter ──
        else if (type == "ELEC_VOLTM") {
            comp->simState["current"] = 0.0;
        }
        // ── Switch ──
        else if (type == "ELEC_SW") {
            const bool closed = comp->properties.value("closed", true).toBool();
            const double R = closed ? 0.001 : 1e9;
            comp->simState["current"] = vDiff / R;
        }
        // ── Fuse ──
        else if (type == "ELEC_FUSE") {
            const double R = comp->destroyed ? 1e9
                : comp->properties.value("resistance", 0.01).toDouble();
            const double I = vDiff / R;
            comp->simState["current"] = I;
            // Blow the fuse if current exceeds rating.
            if (!comp->destroyed) {
                const double rating = comp->properties.value("currentRating", 0.5).toDouble();
                if (std::abs(I) > rating) {
                    comp->destroyed = true;
                }
            }
        }
        // ── Capacitor ──
        else if (type == "ELEC_CAP") {
            const double C    = comp->properties.value("capacitance", 100e-6).toDouble();
            const double G_eq = (dt > 1e-12) ? C / dt : 0.0;
            const double V_prev = comp->simState.value("v_cap", 0.0).toDouble();
            // I = G_eq*(Va-Vb) - G_eq*V_prev
            comp->simState["current"] = G_eq * (vDiff - V_prev);
            comp->simState["v_cap"]   = vDiff;  // update for next tick
        }
        // ── Diode / LED ──
        else if (type == "ELEC_DIODE" || type == "ELEC_LED") {
            const double Vf  = comp->properties.value("forwardVoltage",
                                   (type == "ELEC_LED") ? 2.0 : 0.7).toDouble();
            const double Ron = comp->properties.value("onResistance",
                                   (type == "ELEC_LED") ? 5.0 : 0.1).toDouble();
            const bool glowing = (vDiff > Vf * 0.5);
            const double R = glowing ? Ron : 1e8;
            comp->simState["current"] = vDiff / R;
            comp->simState["glowing"] = glowing;
        }

        // ── Voltage sources (VSRC + AC): current is extra MNA unknown ──
        for (int k = 0; k < vsources.size(); ++k) {
            if (vsources[k] == comp) {
                const int row = (n - 1) + k;
                comp->simState["current"] = (row < x.size()) ? x(row) : 0.0;
            }
        }

        comp->update();
        Q_UNUSED(dt)
    }
}
