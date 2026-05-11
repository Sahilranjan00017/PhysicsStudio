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

void registerElectronicsComponents(ComponentRegistry& registry);
