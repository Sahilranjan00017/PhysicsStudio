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
    // Step 2 — count voltage sources (VSRC, AC, XFMR secondary, logic outputs)
    // -----------------------------------------------------------------------
    int m = 0;
    QList<BaseComponent*> vsources;
    for (auto* comp : domain.components) {
        const QString& t = comp->typeId;
        if (t == "ELEC_VSRC" || t == "ELEC_AC"
         || t == "ELEC_XFMR"
         || t == "ELEC_AND"  || t == "ELEC_OR"  || t == "ELEC_NOT"
         || t == "ELEC_NAND" || t == "ELEC_NOR"  || t == "ELEC_XOR"
         || t == "ELEC_OPAMP") {
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
        // ── Inductor companion model (backward-Euler: G_eq || I_hist) ──
        // V_L = L·dI/dt  →  I_n = I_{n-1} + (dt/L)·V_n
        // Companion: conductance G_eq = dt/L plus current source I_{n-1} (a→b)
        else if (type == "ELEC_IND") {
            const double L    = comp->properties.value("inductance", 0.01).toDouble();
            const double G_eq = (dt > 1e-12 && L > 1e-12) ? dt / L : 0.0;
            const double I_prev = comp->simState.value("i_ind", 0.0).toDouble();

            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

            // Conductance stamp.
            if (a >= 0)     G(a,     a    ) += G_eq;
            if (b_idx >= 0) G(b_idx, b_idx) += G_eq;
            if (a >= 0 && b_idx >= 0) {
                G(a,     b_idx) -= G_eq;
                G(b_idx, a    ) -= G_eq;
            }
            // History current source (a→b direction, magnitude I_prev).
            if (a >= 0)     b(a)     -= I_prev;
            if (b_idx >= 0) b(b_idx) += I_prev;
        }
        // ── NPN Transistor: Rbe resistor + IC controlled current source ──
        else if (type == "ELEC_NPN") {
            if (comp->pads.size() < 3) continue;
            const double hFE  = comp->properties.value("hFE",  100.0).toDouble();
            const double Vth  = comp->properties.value("Vth",    0.6).toDouble();
            const double Rbe  = comp->properties.value("Rbe", 1000.0).toDouble();

            // Pad indices: B=0, C=1, E=2
            const int B = compress(nodeOf(comp->pads[0], nodeMap));
            const int C = compress(nodeOf(comp->pads[1], nodeMap));
            const int E = compress(nodeOf(comp->pads[2], nodeMap));

            // Stamp B-E resistor (base current path).
            const double G_be = 1.0 / std::max(Rbe, 1e-9);
            if (B >= 0) G(B, B) += G_be;
            if (E >= 0) G(E, E) += G_be;
            if (B >= 0 && E >= 0) { G(B, E) -= G_be; G(E, B) -= G_be; }

            // Controlled current source IC = hFE * IB (from C to E).
            const double V_be_prev = comp->simState.value("vBE", 0.0).toDouble();
            if (V_be_prev >= Vth) {
                const double IB = (V_be_prev - Vth) / Rbe;
                const double IC = hFE * IB;
                if (C >= 0) b(C) -= IC;
                if (E >= 0) b(E) += IC;
            }
        }
        // ── PNP Transistor: Rbe resistor + IC controlled current source ──
        // Active when V_EB = V_E − V_B > Vth. Current: IC = hFE·IB, flows E→C.
        else if (type == "ELEC_PNP") {
            if (comp->pads.size() < 3) continue;
            const double hFE = comp->properties.value("hFE",  100.0).toDouble();
            const double Vth = comp->properties.value("Vth",    0.6).toDouble();
            const double Rbe = comp->properties.value("Rbe", 1000.0).toDouble();

            // Pad order: B=0, C=1, E=2
            const int B = compress(nodeOf(comp->pads[0], nodeMap));
            const int C = compress(nodeOf(comp->pads[1], nodeMap));
            const int E = compress(nodeOf(comp->pads[2], nodeMap));

            // Stamp E-B resistor (base current path through emitter-base junction).
            const double G_be = 1.0 / std::max(Rbe, 1e-9);
            if (E >= 0) G(E, E) += G_be;
            if (B >= 0) G(B, B) += G_be;
            if (E >= 0 && B >= 0) { G(E, B) -= G_be; G(B, E) -= G_be; }

            // Controlled current source IC = hFE·IB from emitter to collector.
            const double V_eb_prev = comp->simState.value("vEB", 0.0).toDouble();
            if (V_eb_prev >= Vth) {
                const double IB = (V_eb_prev - Vth) / Rbe;
                const double IC = hFE * IB;
                if (E >= 0) b(E) -= IC;   // current leaves emitter
                if (C >= 0) b(C) += IC;   // current enters collector
            }
        }
        // ── Zener Diode: piecewise-linear forward + reverse-breakdown model ──
        else if (type == "ELEC_ZENER") {
            const double Vf   = comp->properties.value("forwardVoltage", 0.7).toDouble();
            const double Vz   = comp->properties.value("zenerVoltage",   5.1).toDouble();
            const double Ron  = comp->properties.value("onResistance",   0.5).toDouble();
            constexpr double Roff  = 1e8;
            constexpr double Rz_on = 1.0;   // low resistance in Zener breakdown

            if (comp->pads.size() < 2) continue;
            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

            const double V_prev = comp->simState.value("voltageDiff", 0.0).toDouble();

            double G_val;
            double Ioffset = 0.0;
            bool   haveOffset = false;

            if (V_prev > Vf * 0.5) {
                // Forward biased: same as regular diode.
                G_val  = 1.0 / Ron;
                Ioffset = G_val * Vf;
                haveOffset = true;
            } else if (V_prev < -(Vz * 0.5)) {
                // Reverse breakdown: clamp at −Vz.
                G_val   = 1.0 / Rz_on;
                Ioffset = -G_val * Vz;   // negative offset → Va − Vb approaches −Vz
                haveOffset = true;
            } else {
                G_val = 1.0 / Roff;
            }

            if (a >= 0)     G(a,     a    ) += G_val;
            if (b_idx >= 0) G(b_idx, b_idx) += G_val;
            if (a >= 0 && b_idx >= 0) {
                G(a,     b_idx) -= G_val;
                G(b_idx, a    ) -= G_val;
            }
            if (haveOffset) {
                if (a >= 0)     b(a)     += Ioffset;
                if (b_idx >= 0) b(b_idx) -= Ioffset;
            }
        }
        // ── Potentiometer: two series resistors sharing the wiper node ──
        else if (type == "ELEC_POT") {
            const double R_total = std::max(comp->properties.value("resistance", 10000.0).toDouble(), 1.0);
            const double pos     = std::clamp(comp->properties.value("position", 0.5).toDouble(), 0.001, 0.999);

            if (comp->pads.size() < 3) continue;
            const int pa = compress(nodeOf(comp->pads[0], nodeMap));   // a
            const int pw = compress(nodeOf(comp->pads[1], nodeMap));   // wiper
            const int pb = compress(nodeOf(comp->pads[2], nodeMap));   // b

            const double Gaw = 1.0 / (pos         * R_total);
            const double Gwb = 1.0 / ((1.0 - pos) * R_total);

            // Stamp a–wiper segment.
            if (pa >= 0) G(pa, pa) += Gaw;
            if (pw >= 0) G(pw, pw) += Gaw;
            if (pa >= 0 && pw >= 0) { G(pa, pw) -= Gaw; G(pw, pa) -= Gaw; }

            // Stamp wiper–b segment.
            if (pw >= 0) G(pw, pw) += Gwb;
            if (pb >= 0) G(pb, pb) += Gwb;
            if (pw >= 0 && pb >= 0) { G(pw, pb) -= Gwb; G(pb, pw) -= Gwb; }
        }
        // ── Oscilloscope — identical to voltmeter (very high R) ──
        else if (type == "ELEC_SCOPE") {
            const double R   = comp->properties.value("inputResistance", 1e9).toDouble();
            const double G_val = 1.0 / std::max(R, 1e-9);
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
    // Step 4 — stamp voltage sources, AC, Transformer (VCVS), Logic gates
    // -----------------------------------------------------------------------
    for (int k = 0; k < vsources.size(); ++k) {
        auto* comp      = vsources[k];
        const QString& t = comp->typeId;
        const int row   = (n - 1) + k;

        // ── Transformer: VCVS stamp (V_s = n·V_p) ───────────────────────
        if (t == "ELEC_XFMR") {
            if (comp->pads.size() < 4) continue;
            const double ratio = comp->properties.value("ratio", 2.0).toDouble();
            // Pads: p1(0), p2(1) = primary;  s1(2), s2(3) = secondary
            const int p1 = compress(nodeOf(comp->pads[0], nodeMap));
            const int p2 = compress(nodeOf(comp->pads[1], nodeMap));
            const int s1 = compress(nodeOf(comp->pads[2], nodeMap));
            const int s2 = compress(nodeOf(comp->pads[3], nodeMap));

            // VCVS: V(s1)-V(s2) = ratio*(V(p1)-V(p2))
            // KVL row: G[row][s1]+=1; G[row][s2]-=1; G[row][p1]-=ratio; G[row][p2]+=ratio
            if (s1 >= 0) { G(row, s1) += 1.0;    G(s1, row) += 1.0; }
            if (s2 >= 0) { G(row, s2) -= 1.0;    G(s2, row) -= 1.0; }
            if (p1 >= 0)   G(row, p1) -= ratio;
            if (p2 >= 0)   G(row, p2) += ratio;
            b(row) = 0.0;

            // Stamp primary as tiny leakage resistor (avoids floating nodes).
            constexpr double Rleak = 0.01;
            const double Gleak = 1.0 / Rleak;
            if (p1 >= 0) G(p1, p1) += Gleak;
            if (p2 >= 0) G(p2, p2) += Gleak;
            if (p1 >= 0 && p2 >= 0) { G(p1, p2) -= Gleak; G(p2, p1) -= Gleak; }
        }
        // ── Op-Amp: VCVS with high gain, output clamped to supply rails ────
        else if (t == "ELEC_OPAMP") {
            if (comp->pads.size() < 3) continue;
            const double A    = comp->properties.value("gain",       100000.0).toDouble();
            const double Vpos = comp->properties.value("supplyPos",      15.0).toDouble();
            const double Vneg = comp->properties.value("supplyNeg",     -15.0).toDouble();

            // Read last tick's input voltages to compute output this tick.
            const double vP   = comp->simState.value("vPlus",  0.0).toDouble();
            const double vM   = comp->simState.value("vMinus", 0.0).toDouble();
            const double Eraw = A * (vP - vM);
            const double E    = std::clamp(Eraw, Vneg, Vpos);
            comp->simState["outV"] = E;

            // Stamp output as voltage source from out node to ground.
            const int out_a = compress(nodeOf(comp->pads[2], nodeMap));
            if (out_a >= 0) {
                G(out_a, row) += 1.0;
                G(row, out_a) += 1.0;
            }
            b(row) = E;
        }
        // ── Logic gates: output pad driven to Vhigh or Vlow vs GND ─────
        else if (t == "ELEC_AND" || t == "ELEC_OR"  || t == "ELEC_NOT"
              || t == "ELEC_NAND"|| t == "ELEC_NOR"  || t == "ELEC_XOR") {
            if (comp->pads.size() < 2) continue;
            const double Vh   = comp->properties.value("Vhigh",      5.0).toDouble();
            const double Vl   = comp->properties.value("Vlow",       0.0).toDouble();
            const double Vthr = comp->properties.value("Vthreshold", 2.5).toDouble();

            // Read previous input voltages from simState.
            const double v1 = comp->simState.value("in1V", 0.0).toDouble();
            const double v2 = comp->simState.value("in2V", 0.0).toDouble();
            const bool   h1 = (v1 >= Vthr);
            const bool   h2 = (v2 >= Vthr);

            bool outHigh = false;
            if      (t == "ELEC_AND")  outHigh =  h1 && h2;
            else if (t == "ELEC_OR")   outHigh =  h1 || h2;
            else if (t == "ELEC_NAND") outHigh = !(h1 && h2);
            else if (t == "ELEC_NOR")  outHigh = !(h1 || h2);
            else if (t == "ELEC_XOR")  outHigh =  h1 != h2;
            else                       outHigh = !h1;         // NOT uses in1 only

            const double E = outHigh ? Vh : Vl;
            comp->simState["outV"] = E;

            // Output pad is the last pad.
            const int out_a = compress(nodeOf(comp->pads.last(), nodeMap));
            if (out_a >= 0) {
                G(out_a, row) += 1.0;
                G(row, out_a) += 1.0;
            }
            b(row) = E;
        }
        // ── DC / AC voltage sources ──────────────────────────────────────
        else {
            double E;
            if (t == "ELEC_AC") {
                const double Vamp = comp->properties.value("voltage",    5.0).toDouble();
                const double freq = comp->properties.value("frequency", 50.0).toDouble();
                E = Vamp * std::sin(2.0 * std::numbers::pi * freq * domain.simTime);
            } else {
                E = comp->properties.value("voltage", 5.0).toDouble();
            }
            if (comp->pads.size() < 2) continue;

            const int a     = compress(nodeOf(comp->pads[0], nodeMap));
            const int b_idx = compress(nodeOf(comp->pads[1], nodeMap));

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

        // ── Inductor — update history current ──
        else if (type == "ELEC_IND") {
            const double L    = comp->properties.value("inductance", 0.01).toDouble();
            const double G_eq = (dt > 1e-12 && L > 1e-12) ? dt / L : 0.0;
            const double I_prev = comp->simState.value("i_ind", 0.0).toDouble();
            const double I_new  = G_eq * vDiff + I_prev;
            comp->simState["current"] = I_new;
            comp->simState["i_ind"]   = I_new;  // history for next tick
        }
        // ── NPN Transistor ──
        else if (type == "ELEC_NPN") {
            if (comp->pads.size() < 3) continue;
            const double hFE  = comp->properties.value("hFE",  100.0).toDouble();
            const double Vth  = comp->properties.value("Vth",    0.6).toDouble();
            const double Rbe  = comp->properties.value("Rbe", 1000.0).toDouble();

            const int nodeB = nodeOf(comp->pads[0], nodeMap);
            const int nodeC = nodeOf(comp->pads[1], nodeMap);
            const int nodeE = nodeOf(comp->pads[2], nodeMap);

            const double vB = (nodeB >= 0 && nodeB < n) ? V(nodeB) : 0.0;
            const double vC = (nodeC >= 0 && nodeC < n) ? V(nodeC) : 0.0;
            const double vE = (nodeE >= 0 && nodeE < n) ? V(nodeE) : 0.0;
            const double vBE = vB - vE;

            comp->simState["vBE"] = vBE;
            comp->simState["voltageA"] = vB;
            comp->simState["voltageB"] = vE;

            if (vBE >= Vth) {
                const double IB = (vBE - Vth) / Rbe;
                const double IC = hFE * IB;
                comp->simState["iB"]     = IB;
                comp->simState["iC"]     = IC;
                comp->simState["current"] = IC;
                comp->simState["region"] = "active";
            } else {
                comp->simState["iB"]     = 0.0;
                comp->simState["iC"]     = 0.0;
                comp->simState["current"] = 0.0;
                comp->simState["region"] = "off";
            }
            comp->simState["vCE"] = vC - vE;
        }
        // ── PNP Transistor readback ──
        else if (type == "ELEC_PNP") {
            if (comp->pads.size() < 3) continue;
            const double hFE = comp->properties.value("hFE",  100.0).toDouble();
            const double Vth = comp->properties.value("Vth",    0.6).toDouble();
            const double Rbe = comp->properties.value("Rbe", 1000.0).toDouble();

            const int nodeB_p = nodeOf(comp->pads[0], nodeMap);
            const int nodeC_p = nodeOf(comp->pads[1], nodeMap);
            const int nodeE_p = nodeOf(comp->pads[2], nodeMap);

            const double vB_p = (nodeB_p >= 0 && nodeB_p < n) ? V(nodeB_p) : 0.0;
            const double vC_p = (nodeC_p >= 0 && nodeC_p < n) ? V(nodeC_p) : 0.0;
            const double vE_p = (nodeE_p >= 0 && nodeE_p < n) ? V(nodeE_p) : 0.0;
            const double vEB  = vE_p - vB_p;

            comp->simState["vEB"]     = vEB;
            comp->simState["voltageA"] = vB_p;
            comp->simState["voltageB"] = vE_p;
            comp->simState["vCE"]     = vC_p - vE_p;

            if (vEB >= Vth) {
                const double IB = (vEB - Vth) / Rbe;
                const double IC = hFE * IB;
                comp->simState["iB"]      = IB;
                comp->simState["iC"]      = IC;
                comp->simState["current"]  = IC;
                comp->simState["region"]   = "active";
            } else {
                comp->simState["iB"]      = 0.0;
                comp->simState["iC"]      = 0.0;
                comp->simState["current"]  = 0.0;
                comp->simState["region"]   = "off";
            }
        }
        // ── Zener Diode readback ──
        else if (type == "ELEC_ZENER") {
            const double Vf  = comp->properties.value("forwardVoltage", 0.7).toDouble();
            const double Vz  = comp->properties.value("zenerVoltage",   5.1).toDouble();
            const double Ron = comp->properties.value("onResistance",   0.5).toDouble();
            constexpr double Rz_on = 1.0;
            constexpr double Roff  = 1e8;

            const bool fwdOn = (vDiff > Vf * 0.5);
            const bool zenOn = (vDiff < -(Vz * 0.5));
            const double R   = fwdOn ? Ron : zenOn ? Rz_on : Roff;
            comp->simState["current"] = vDiff / R;
        }
        // ── Potentiometer readback ──
        else if (type == "ELEC_POT") {
            if (comp->pads.size() < 3) continue;
            const int nodeW = nodeOf(comp->pads[1], nodeMap);
            const double vW = (nodeW >= 0 && nodeW < n) ? V(nodeW) : 0.0;
            comp->simState["voltageWiper"] = vW;
            comp->simState["current"] = 0.0;
        }
        // ── Op-Amp readback: store input node voltages for next tick ──
        else if (type == "ELEC_OPAMP") {
            if (comp->pads.size() < 3) continue;
            const int nInP = nodeOf(comp->pads[0], nodeMap);
            const int nInM = nodeOf(comp->pads[1], nodeMap);
            const double vP = (nInP >= 0 && nInP < n) ? V(nInP) : 0.0;
            const double vM = (nInM >= 0 && nInM < n) ? V(nInM) : 0.0;
            comp->simState["vPlus"]  = vP;
            comp->simState["vMinus"] = vM;
        }
        // ── Logic gate input voltage readback ──
        else if (type == "ELEC_AND"  || type == "ELEC_OR"   || type == "ELEC_NOT"
              || type == "ELEC_NAND" || type == "ELEC_NOR"   || type == "ELEC_XOR") {
            // Store input node voltages for next tick's gate computation.
            if (comp->pads.size() >= 2) {
                const int n_in1 = nodeOf(comp->pads[0], nodeMap);
                const double v_in1 = (n_in1 >= 0 && n_in1 < n) ? V(n_in1) : 0.0;
                comp->simState["in1V"] = v_in1;
                if (comp->pads.size() >= 3) {
                    const int n_in2 = nodeOf(comp->pads[1], nodeMap);
                    const double v_in2 = (n_in2 >= 0 && n_in2 < n) ? V(n_in2) : 0.0;
                    comp->simState["in2V"] = v_in2;
                }
            }
        }
        // ── Oscilloscope — voltmeter-like readback ──
        else if (type == "ELEC_SCOPE") {
            comp->simState["current"] = 0.0;
            // voltageDiff already written above — that's all the scope needs
        }
        // ── Transformer: write primary/secondary voltages ──
        else if (type == "ELEC_XFMR") {
            if (comp->pads.size() < 4) continue;
            const int np1 = nodeOf(comp->pads[0], nodeMap);
            const int np2 = nodeOf(comp->pads[1], nodeMap);
            const int ns1 = nodeOf(comp->pads[2], nodeMap);
            const int ns2 = nodeOf(comp->pads[3], nodeMap);

            const double vp1 = (np1>=0 && np1<n) ? V(np1) : 0.0;
            const double vp2 = (np2>=0 && np2<n) ? V(np2) : 0.0;
            const double vs1 = (ns1>=0 && ns1<n) ? V(ns1) : 0.0;
            const double vs2 = (ns2>=0 && ns2<n) ? V(ns2) : 0.0;

            comp->simState["primaryVoltage"]   = vp1 - vp2;
            comp->simState["secondaryVoltage"]  = vs1 - vs2;
            comp->simState["voltageDiff"]       = vp1 - vp2;  // primary for display
        }

        // ── Voltage sources (VSRC, AC, OPAMP, gates): current is extra MNA unknown ──
        for (int k = 0; k < vsources.size(); ++k) {
            if (vsources[k] == comp) {
                const int row = (n - 1) + k;
                if (row < x.size())
                    comp->simState["current"] = x(row);
                // Transformer secondary current.
                if (comp->typeId == "ELEC_XFMR")
                    comp->simState["secondaryCurrent"] = (row < x.size()) ? x(row) : 0.0;
            }
        }

        // update() schedules a repaint — must run on the main (GUI) thread.
        QMetaObject::invokeMethod(comp, [comp]() { comp->update(); },
                                  Qt::QueuedConnection);
        Q_UNUSED(dt)
    }
}
