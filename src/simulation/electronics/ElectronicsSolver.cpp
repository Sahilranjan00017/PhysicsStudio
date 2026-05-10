#include "simulation/electronics/ElectronicsSolver.h"

#include "components/BaseComponent.h"
#include "components/ConnectionPad.h"
#include "components/Wire.h"

#include <Eigen/Dense>

#include <QDebug>
#include <QMap>

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

    // -----------------------------------------------------------------------
    // Step 1 — build node map
    // -----------------------------------------------------------------------
    int n = 0;
    NodeMap nodeMap = buildNodeMap(domain, n);

    int groundNode = findGroundNode(domain, nodeMap);
    if (groundNode < 0) return; // already warned

    // -----------------------------------------------------------------------
    // Step 2 — count voltage sources (need extra unknowns)
    // -----------------------------------------------------------------------
    int m = 0;
    QList<BaseComponent*> vsources;
    for (auto* comp : domain.components) {
        if (comp->typeId == "ELEC_VSRC") {
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
    // Step 3 — stamp resistors, ammeters, voltmeters, current sources
    // -----------------------------------------------------------------------
    for (auto* comp : domain.components) {
        const QString& type = comp->typeId;

        if (type == "ELEC_RES" || type == "ELEC_AMM" || type == "ELEC_VOLTM") {
            // All three stamp as a conductance.
            double R = 1.0;
            if (type == "ELEC_RES")
                R = comp->properties.value("resistance", 1000.0).toDouble();
            else if (type == "ELEC_AMM")
                R = comp->properties.value("burdenResistance", 1e-6).toDouble();
            else // voltmeter — very high input resistance
                R = comp->properties.value("inputResistance", 1e9).toDouble();

            if (R <= 0.0) R = 1e-9; // clamp to avoid divide-by-zero
            const double G_val = 1.0 / R;

            if (comp->pads.size() < 2) continue;
            const int a = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

            if (a >= 0)     G(a,     a    ) += G_val;
            if (b_idx >= 0) G(b_idx, b_idx) += G_val;
            if (a >= 0 && b_idx >= 0) {
                G(a,     b_idx) -= G_val;
                G(b_idx, a    ) -= G_val;
            }
        }
        else if (type == "ELEC_ISRC") {
            // Current source stamps into RHS only.
            const double I = comp->properties.value("current", 0.01).toDouble();
            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));
            // Conventional current flows a → b (out of pad[0], into pad[1]).
            if (a >= 0)     b(a)     -= I;
            if (b_idx >= 0) b(b_idx) += I;
        }
        // Ground and voltage sources handled separately.
    }

    // -----------------------------------------------------------------------
    // Step 4 — stamp voltage sources
    // -----------------------------------------------------------------------
    for (int k = 0; k < vsources.size(); ++k) {
        auto* comp  = vsources[k];
        const double E = comp->properties.value("voltage", 5.0).toDouble();
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

        comp->simState["voltageA"] = vA;
        comp->simState["voltageB"] = vB;
        comp->simState["voltageDiff"] = vA - vB;

        // Calculate current for resistive elements.
        const QString& type = comp->typeId;
        if (type == "ELEC_RES" || type == "ELEC_AMM") {
            const double R = type == "ELEC_RES"
                ? comp->properties.value("resistance",     1000.0).toDouble()
                : comp->properties.value("burdenResistance", 1e-6).toDouble();
            comp->simState["current"] = (R > 0.0) ? (vA - vB) / R : 0.0;
        }
        else if (type == "ELEC_VOLTM") {
            comp->simState["current"] = 0.0;
        }

        // Voltage source: current is the extra unknown x[n-1+k].
        for (int k = 0; k < vsources.size(); ++k) {
            if (vsources[k] == comp) {
                const int row = (n - 1) + k;
                comp->simState["current"] = (row < x.size()) ? x(row) : 0.0;
            }
        }

        // Fire update signal so Properties Panel refreshes.
        Q_UNUSED(dt)
    }
}
