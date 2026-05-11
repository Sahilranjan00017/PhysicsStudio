#pragma once

#include "components/BaseComponent.h"

#include <QRectF>

#include <array>

class ComponentRegistry;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class TwoTerminalElectricalComponent : public BaseComponent {
    Q_OBJECT

public:
    explicit TwoTerminalElectricalComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;

protected:
    void drawTerminals(QPainter* painter) const;
    void drawLabel(QPainter* painter) const;

private:
    std::array<ConnectionPad, 2> padStorage;
};

class ResistorComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit ResistorComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class VoltageSourceComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit VoltageSourceComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class CurrentSourceComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit CurrentSourceComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class AmmeterComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit AmmeterComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class VoltmeterComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit VoltmeterComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class GroundComponent final : public BaseComponent {
    Q_OBJECT

public:
    explicit GroundComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    ConnectionPad groundPad;
};

// ---------------------------------------------------------------------------
// CapacitorComponent  (ELEC_CAP)
// Stamped with backward-Euler companion model: G_eq = C/dt, I_hist current src.
// Properties: capacitance (F)
// simState  : v_cap (voltage across capacitor, updated each tick)
// ---------------------------------------------------------------------------
class CapacitorComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit CapacitorComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// LEDComponent  (ELEC_LED)
// Piecewise-linear diode; glows when forward-biased.
// Properties: forwardVoltage (V), onResistance (Ω), color (nm wavelength)
// simState  : voltageDiff, current, glowing (bool)
// ---------------------------------------------------------------------------
class LEDComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit LEDComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// SwitchComponent  (ELEC_SW)
// Open → very high R; closed → very low R.
// Properties: closed (bool)
// ---------------------------------------------------------------------------
class SwitchComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit SwitchComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// DiodeComponent  (ELEC_DIODE)
// Piecewise-linear ideal diode with threshold voltage.
// Properties: forwardVoltage (V), onResistance (Ω)
// simState  : voltageDiff, current
// ---------------------------------------------------------------------------
class DiodeComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit DiodeComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// ACSourceComponent  (ELEC_AC)
// Sinusoidal voltage source V = Vamp · sin(2π·f·t).
// Stamped as a voltage source with time-varying E; needs domain.simTime.
// Properties: voltage (Vamp, V), frequency (Hz)
// ---------------------------------------------------------------------------
class ACSourceComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit ACSourceComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// FuseComponent  (ELEC_FUSE)
// Stamped as a resistor; self-destructs when I > rating.
// Properties: resistance (Ω), currentRating (A)
// simState  : current; destroyed flag triggers visual change
// ---------------------------------------------------------------------------
class FuseComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit FuseComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// InductorComponent  (ELEC_IND)
// Backward-Euler companion model: G_eq = dt/L, I_hist current source.
// Properties: inductance (H)
// simState  : i_ind (current through inductor, updated each tick)
// ---------------------------------------------------------------------------
class InductorComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit InductorComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// TransformerComponent  (ELEC_XFMR)
// Ideal 1:n transformer — secondary stamped as VCVS: Vs = n·Vp.
// 4 terminals: p1(+), p2(−) primary; s1(+), s2(−) secondary.
// Properties: ratio (n = Nsecondary/Nprimary)
// simState  : primaryVoltage, secondaryVoltage, secondaryCurrent
// ---------------------------------------------------------------------------
class TransformerComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit TransformerComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 4> padStorage;  // p1, p2, s1, s2
};

// ---------------------------------------------------------------------------
// NPNTransistorComponent  (ELEC_NPN)
// DC operating-point BJT model: IC = hFE · IB (current-controlled source).
// 3 terminals: B (base), C (collector), E (emitter).
// Properties: hFE (current gain β), Vth (base threshold V), Rbe (Ω)
// simState  : vBE, iC, region ("off"/"active")
// ---------------------------------------------------------------------------
class NPNTransistorComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit NPNTransistorComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 3> padStorage;  // B, C, E
};

// ---------------------------------------------------------------------------
// PNPTransistorComponent  (ELEC_PNP)
// Complementary BJT to NPN: active when V_EB > Vth.
// IC = hFE·IB flows from emitter to collector.
// 3 terminals: B (base), C (collector), E (emitter).
// Properties: hFE, Vth (V), Rbe (Ω)
// simState  : vEB, iC, region ("off"/"active")
// ---------------------------------------------------------------------------
class PNPTransistorComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit PNPTransistorComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 3> padStorage;  // B, C, E
};

// ---------------------------------------------------------------------------
// OpAmpComponent  (ELEC_OPAMP)
// Ideal op-amp: Vout = clamp(A·(V+ − V−), Vneg, Vpos).
// 3 main terminals: in+ (non-inverting), in- (inverting), out.
// Properties: gain (A, default 1e5), supplyPos (V), supplyNeg (V)
// simState  : vPlus, vMinus, outV
// ---------------------------------------------------------------------------
class OpAmpComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit OpAmpComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 3> padStorage;  // in+, in-, out
};

// ---------------------------------------------------------------------------
// ZenerDiodeComponent  (ELEC_ZENER)
// Forward bias: conducts at Vf (like regular diode).
// Reverse breakdown: clamps at Vz (zener voltage).
// Properties: forwardVoltage (V), zenerVoltage (V), onResistance (Ω)
// simState  : voltageDiff, current
// ---------------------------------------------------------------------------
class ZenerDiodeComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT
public:
    explicit ZenerDiodeComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

// ---------------------------------------------------------------------------
// PotentiometerComponent  (ELEC_POT)
// Three-terminal variable resistor: A—[R·pos]—Wiper—[R·(1−pos)]—B.
// Properties: resistance (Ω total), position (0–1, wiper fraction from A)
// simState  : voltageA, voltageB, voltageWiper
// ---------------------------------------------------------------------------
class PotentiometerComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit PotentiometerComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 3> padStorage;  // a, wiper, b
};

// ---------------------------------------------------------------------------
// LogicGateComponent  (ELEC_AND / ELEC_OR / ELEC_NOT / ELEC_NAND / ELEC_NOR / ELEC_XOR)
// Digital gate: inputs read voltage levels; output drives a controlled voltage.
// 2-input gates (AND,OR,NAND,NOR,XOR): in1, in2, out pads.
// Single-input NOT: in, out pads.
// Properties: Vhigh (5.0), Vlow (0.0), Vthreshold (2.5)
// simState  : in1V, in2V (voltage levels), outV (driven output)
// ---------------------------------------------------------------------------
class LogicGateComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit LogicGateComponent(const QString& typeId, QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 3> padStorage;  // in1, in2 (or in), out
    int m_numInputs = 2;
};

// ---------------------------------------------------------------------------
// OscilloscopeComponent  (ELEC_SCOPE)
// Two-terminal waveform viewer — identical MNA stamp to voltmeter.
// Displays a scrolling waveform trace on its body.
// simState  : voltageDiff (updated each tick by solver)
// DataLogger feeds ELEC_SCOPE voltageDiff as "scope" channel.
// ---------------------------------------------------------------------------
class OscilloscopeComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit OscilloscopeComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
private:
    std::array<ConnectionPad, 2> padStorage;  // ch+ and ch-
};

void registerElectronicsComponents(ComponentRegistry& registry);
