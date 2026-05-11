#include "components/electronics/ElectronicsComponents.h"

#include "components/ComponentRegistry.h"

#include <QPainter>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>

namespace {
constexpr double bodyWidth = 96.0;
constexpr double terminalOffset = bodyWidth / 2.0;

QPen componentPen(bool selected)
{
    return QPen(selected ? QColor(28, 100, 242) : QColor(35, 42, 50), selected ? 2.5 : 2.0);
}

QBrush componentBrush(bool destroyed)
{
    return destroyed ? QBrush(QColor(255, 235, 235)) : QBrush(QColor(255, 255, 255));
}

void drawCircleWithText(QPainter* painter, const QRectF& circle, const QString& text)
{
    painter->drawEllipse(circle);
    painter->drawText(circle, Qt::AlignCenter, text);
}

ComponentDescriptor descriptor(
    const QString& typeId,
    const QString& displayName,
    const QString& description,
    const QMap<QString, QVariant>& defaultProperties = {})
{
    return ComponentDescriptor {
        typeId,
        displayName,
        "Electronics/Analog",
        description,
        defaultProperties,
    };
}
} // namespace

TwoTerminalElectricalComponent::TwoTerminalElectricalComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage {
          ConnectionPad { QPointF(-terminalOffset, 0.0), PadType::Bidirectional, DomainType::Electrical, "a", {}, false },
          ConnectionPad { QPointF(terminalOffset, 0.0), PadType::Bidirectional, DomainType::Electrical, "b", {}, false },
      }
{
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
}

QRectF TwoTerminalElectricalComponent::boundingRect() const
{
    return QRectF(-60.0, -32.0, 120.0, 64.0);
}

void TwoTerminalElectricalComponent::drawTerminals(QPainter* painter) const
{
    painter->drawLine(QPointF(-60.0, 0.0), QPointF(-36.0, 0.0));
    painter->drawLine(QPointF(36.0, 0.0), QPointF(60.0, 0.0));
    painter->setBrush(QColor(28, 100, 242));
    painter->drawEllipse(QPointF(-60.0, 0.0), 3.5, 3.5);
    painter->drawEllipse(QPointF(60.0, 0.0), 3.5, 3.5);
}

void TwoTerminalElectricalComponent::drawLabel(QPainter* painter) const
{
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-50.0, 18.0, 100.0, 18.0), Qt::AlignCenter, displayName);
}

ResistorComponent::ResistorComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId = "ELEC_RES";
    displayName = "Resistor";
}

void ResistorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    const QPolygonF zigzag {
        QPointF(-36.0, 0.0), QPointF(-26.0, -12.0), QPointF(-14.0, 12.0),
        QPointF(-2.0, -12.0), QPointF(10.0, 12.0), QPointF(22.0, -12.0),
        QPointF(36.0, 0.0),
    };
    painter->drawPolyline(zigzag);
    drawLabel(painter);
}

VoltageSourceComponent::VoltageSourceComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId = "ELEC_VSRC";
    displayName = "Voltage Source";
}

void VoltageSourceComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);
    drawCircleWithText(painter, QRectF(-18.0, -18.0, 36.0, 36.0), "V");
    painter->drawText(QRectF(12.0, -24.0, 14.0, 14.0), Qt::AlignCenter, "+");
    painter->drawText(QRectF(-26.0, -24.0, 14.0, 14.0), Qt::AlignCenter, "-");
    drawLabel(painter);
}

CurrentSourceComponent::CurrentSourceComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId = "ELEC_ISRC";
    displayName = "Current Source";
}

void CurrentSourceComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);
    painter->drawEllipse(QRectF(-18.0, -18.0, 36.0, 36.0));
    painter->drawLine(QPointF(0.0, 12.0), QPointF(0.0, -10.0));
    painter->drawLine(QPointF(0.0, -10.0), QPointF(-6.0, -3.0));
    painter->drawLine(QPointF(0.0, -10.0), QPointF(6.0, -3.0));
    drawLabel(painter);
}

AmmeterComponent::AmmeterComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId = "ELEC_AMM";
    displayName = "Ammeter";
}

QRectF AmmeterComponent::boundingRect() const
{
    return QRectF(-68.0, -32.0, 136.0, 64.0);  // wider for reading text
}

void AmmeterComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    // Show live current reading if simulation is running.
    const bool hasReading = simState.contains("current");
    if (hasReading) {
        const double I = simState["current"].toDouble();
        // Scale to mA if small.
        const QString reading = (qAbs(I) < 0.9995)
            ? QString("%1 mA").arg(I * 1000.0, 0, 'f', 2)
            : QString("%1 A").arg(I,           0, 'f', 3);
        drawCircleWithText(painter, QRectF(-28.0, -18.0, 56.0, 36.0), reading);
    } else {
        drawCircleWithText(painter, QRectF(-28.0, -18.0, 56.0, 36.0), "A");
    }
    drawLabel(painter);
}

VoltmeterComponent::VoltmeterComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId = "ELEC_VOLTM";
    displayName = "Voltmeter";
}

QRectF VoltmeterComponent::boundingRect() const
{
    return QRectF(-68.0, -32.0, 136.0, 64.0);  // wider for reading text
}

void VoltmeterComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    // Show live voltage reading if simulation is running.
    const bool hasReading = simState.contains("voltageDiff");
    if (hasReading) {
        const double V = simState["voltageDiff"].toDouble();
        const QString reading = (qAbs(V) < 0.9995)
            ? QString("%1 mV").arg(V * 1000.0, 0, 'f', 1)
            : QString("%1 V").arg(V,           0, 'f', 3);
        drawCircleWithText(painter, QRectF(-28.0, -18.0, 56.0, 36.0), reading);
    } else {
        drawCircleWithText(painter, QRectF(-28.0, -18.0, 56.0, 36.0), "V");
    }
    drawLabel(painter);
}

GroundComponent::GroundComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      groundPad { QPointF(0.0, -24.0), PadType::Bidirectional, DomainType::Electrical, "gnd", {}, false }
{
    typeId = "ELEC_GND";
    displayName = "Ground";
    pads.append(&groundPad);
}

QRectF GroundComponent::boundingRect() const
{
    return QRectF(-32.0, -28.0, 64.0, 56.0);
}

void GroundComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    painter->drawLine(QPointF(0.0, -24.0), QPointF(0.0, -4.0));
    painter->drawLine(QPointF(-22.0, -4.0), QPointF(22.0, -4.0));
    painter->drawLine(QPointF(-14.0, 6.0), QPointF(14.0, 6.0));
    painter->drawLine(QPointF(-7.0, 16.0), QPointF(7.0, 16.0));
    painter->setBrush(QColor(28, 100, 242));
    painter->drawEllipse(QPointF(0.0, -24.0), 3.5, 3.5);
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-32.0, 20.0, 64.0, 18.0), Qt::AlignCenter, displayName);
}

// ===========================================================================
// CapacitorComponent
// ===========================================================================

CapacitorComponent::CapacitorComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_CAP";
    displayName = "Capacitor";
}

void CapacitorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    // Parallel-plate capacitor symbol: two vertical bars separated by a gap.
    painter->drawLine(QPointF(-36.0, 0.0), QPointF(-6.0, 0.0));
    painter->drawLine(QPointF(  6.0, 0.0), QPointF(36.0, 0.0));
    painter->setPen(QPen(componentPen(option->state & QStyle::State_Selected).color(), 3.5));
    painter->drawLine(QPointF(-6.0, -16.0), QPointF(-6.0, 16.0));
    painter->drawLine(QPointF( 6.0, -16.0), QPointF( 6.0, 16.0));

    // Voltage label across plates.
    if (simState.contains("voltageDiff")) {
        const double V = simState["voltageDiff"].toDouble();
        painter->setPen(QColor(35, 42, 50));
        painter->setFont(QFont("Arial", 8));
        painter->drawText(QRectF(-50.0, 18.0, 100.0, 16.0), Qt::AlignCenter,
                          QString("%1 V").arg(V, 0, 'f', 2));
    } else {
        drawLabel(painter);
    }
}

// ===========================================================================
// LEDComponent
// ===========================================================================

LEDComponent::LEDComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_LED";
    displayName = "LED";
}

void LEDComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    drawTerminals(painter);

    const bool glowing = simState.value("glowing", false).toBool();
    const double wavelength = properties.value("wavelength", 625.0).toDouble();

    // Glow halo when forward biased.
    if (glowing) {
        QColor halo;
        if      (wavelength < 450) halo = QColor(80, 0, 255, 60);
        else if (wavelength < 520) halo = QColor(0, 200, 0, 60);
        else if (wavelength < 590) halo = QColor(255, 200, 0, 60);
        else                       halo = QColor(255, 50, 0, 60);
        painter->setBrush(halo);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(0.0, 0.0), 26.0, 26.0);
        painter->setPen(componentPen(option->state & QStyle::State_Selected));
    }

    // Diode triangle body.
    QColor bodyColor = glowing ? QColor(255, 220, 180) : componentBrush(destroyed).color();
    if (glowing && wavelength < 450)      bodyColor = QColor(200, 180, 255);
    else if (glowing && wavelength < 520) bodyColor = QColor(180, 255, 200);
    else if (glowing && wavelength < 590) bodyColor = QColor(255, 255, 180);
    painter->setBrush(bodyColor);

    const QPolygonF triangle {
        QPointF(-18.0, -14.0), QPointF(-18.0, 14.0), QPointF(14.0, 0.0)
    };
    painter->drawPolygon(triangle);
    painter->drawLine(QPointF(14.0, -14.0), QPointF(14.0, 14.0));
    painter->drawLine(QPointF(-36.0, 0.0), QPointF(-18.0, 0.0));
    painter->drawLine(QPointF(14.0, 0.0), QPointF(36.0, 0.0));

    // Emission arrows.
    if (glowing) {
        painter->setPen(QPen(QColor(255, 200, 0), 1.5));
        painter->drawLine(QPointF(18.0, -8.0), QPointF(28.0, -18.0));
        painter->drawLine(QPointF(24.0, -4.0), QPointF(34.0, -14.0));
    }

    drawLabel(painter);
}

// ===========================================================================
// SwitchComponent
// ===========================================================================

SwitchComponent::SwitchComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_SW";
    displayName = "Switch";
}

void SwitchComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(Qt::NoBrush);
    drawTerminals(painter);

    const bool closed = properties.value("closed", true).toBool();

    // Pivot dots.
    painter->setBrush(QColor(35, 42, 50));
    painter->drawEllipse(QPointF(-18.0, 0.0), 4.0, 4.0);
    painter->drawEllipse(QPointF( 18.0, 0.0), 4.0, 4.0);
    painter->setBrush(Qt::NoBrush);

    if (closed) {
        // Closed: straight horizontal connecting bar.
        painter->drawLine(QPointF(-18.0, 0.0), QPointF(18.0, 0.0));
    } else {
        // Open: bar tilted upward from left pivot.
        painter->drawLine(QPointF(-18.0, 0.0), QPointF(14.0, -14.0));
    }

    // Label shows state.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-50.0, 18.0, 100.0, 16.0), Qt::AlignCenter,
                      closed ? "SW (closed)" : "SW (open)");
}

// ===========================================================================
// DiodeComponent
// ===========================================================================

DiodeComponent::DiodeComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_DIODE";
    displayName = "Diode";
}

void DiodeComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    // Standard diode symbol: triangle + bar.
    const QPolygonF triangle {
        QPointF(-18.0, -14.0), QPointF(-18.0, 14.0), QPointF(14.0, 0.0)
    };
    painter->drawPolygon(triangle);
    painter->drawLine(QPointF(14.0, -14.0), QPointF(14.0, 14.0));
    painter->drawLine(QPointF(-36.0, 0.0), QPointF(-18.0, 0.0));
    painter->drawLine(QPointF(14.0,  0.0), QPointF(36.0,  0.0));

    drawLabel(painter);
}

// ===========================================================================
// ACSourceComponent
// ===========================================================================

ACSourceComponent::ACSourceComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_AC";
    displayName = "AC Source";
}

void ACSourceComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    // Circle body.
    painter->drawEllipse(QRectF(-18.0, -18.0, 36.0, 36.0));

    // Sine-wave symbol inside (~).
    painter->setPen(QPen(QColor(35, 42, 50), 1.5));
    const double wx = 7.0, wy = 5.0;
    painter->drawArc(QRectF(-wx, -wy, wx, 2.0 * wy),      0, 180 * 16);
    painter->drawArc(QRectF(0.0, -wy, wx, 2.0 * wy), 180 * 16, 180 * 16);

    // Frequency label.
    const double freq = properties.value("frequency", 50.0).toDouble();
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-50.0, 18.0, 100.0, 16.0), Qt::AlignCenter,
                      QString("%1 Hz").arg(freq, 0, 'f', 0));
}

// ===========================================================================
// FuseComponent
// ===========================================================================

FuseComponent::FuseComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_FUSE";
    displayName = "Fuse";
}

void FuseComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);

    // Fuse body: rectangle.
    painter->drawRect(QRectF(-22.0, -10.0, 44.0, 20.0));
    painter->drawLine(QPointF(-36.0, 0.0), QPointF(-22.0, 0.0));
    painter->drawLine(QPointF( 22.0, 0.0), QPointF( 36.0, 0.0));

    if (destroyed) {
        // Blown fuse: broken wire inside.
        painter->setPen(QPen(QColor(200, 60, 60), 2.0));
        painter->drawLine(QPointF(-16.0, 0.0), QPointF(-6.0,  0.0));
        painter->drawLine(QPointF(  6.0, 0.0), QPointF(16.0,  0.0));
        painter->drawLine(QPointF( -6.0, 0.0), QPointF( -2.0, -7.0));
        painter->drawLine(QPointF(  2.0,-7.0), QPointF(  6.0,  0.0));
    } else {
        // Intact wire inside.
        painter->setPen(QPen(QColor(35, 42, 50), 1.5));
        painter->drawLine(QPointF(-16.0, 0.0), QPointF(16.0, 0.0));
    }

    drawLabel(painter);
}

// ===========================================================================
// Registration
// ===========================================================================

void registerElectronicsComponents(ComponentRegistry& registry)
{
    registry.registerType(
        descriptor("ELEC_RES", "Resistor", "Limits current using Ohm's law", {
            { "resistance", 1000.0 },
            { "maxCurrent", 0.25 },
            { "destructible", true },
        }),
        [] { return new ResistorComponent(); });

    registry.registerType(
        descriptor("ELEC_GND", "Ground", "Defines the 0 V reference node"),
        [] { return new GroundComponent(); });

    registry.registerType(
        descriptor("ELEC_VSRC", "Voltage Source", "Ideal DC voltage source", {
            { "voltage", 5.0 },
            { "internalResistance", 0.0 },
        }),
        [] { return new VoltageSourceComponent(); });

    registry.registerType(
        descriptor("ELEC_ISRC", "Current Source", "Ideal DC current source", {
            { "current", 0.01 },
        }),
        [] { return new CurrentSourceComponent(); });

    registry.registerType(
        descriptor("ELEC_AMM", "Ammeter", "Measures branch current", {
            { "range", 1.0 },
            { "burdenResistance", 0.000001 },
        }),
        [] { return new AmmeterComponent(); });

    registry.registerType(
        descriptor("ELEC_VOLTM", "Voltmeter", "Measures voltage between two nodes", {
            { "range", 10.0 },
            { "inputResistance", 1000000000.0 },
        }),
        [] { return new VoltmeterComponent(); });

    registry.registerType(
        descriptor("ELEC_CAP", "Capacitor",
                   "Stores charge; transient RC charging/discharging (backward-Euler model)", {
            { "capacitance", 0.0001 },  // 100 µF
        }),
        [] { return new CapacitorComponent(); });

    registry.registerType(
        descriptor("ELEC_LED", "LED",
                   "Light-emitting diode — glows when forward biased", {
            { "forwardVoltage", 2.0 },
            { "onResistance",   5.0 },
            { "wavelength",   625.0 },  // nm (red default)
        }),
        [] { return new LEDComponent(); });

    registry.registerType(
        descriptor("ELEC_SW", "Switch",
                   "Manually open or close a circuit branch", {
            { "closed", true },
        }),
        [] { return new SwitchComponent(); });

    registry.registerType(
        descriptor("ELEC_DIODE", "Diode",
                   "One-way current valve with threshold voltage", {
            { "forwardVoltage", 0.7 },
            { "onResistance",   0.1 },
        }),
        [] { return new DiodeComponent(); });

    registry.registerType(
        descriptor("ELEC_AC", "AC Source",
                   "Sinusoidal voltage source V = Vamp·sin(2π·f·t)", {
            { "voltage",   5.0 },   // peak amplitude (V)
            { "frequency", 50.0 },  // Hz
        }),
        [] { return new ACSourceComponent(); });

    registry.registerType(
        descriptor("ELEC_FUSE", "Fuse",
                   "Protects circuit — blows (self-destructs) when I > rating", {
            { "resistance",     0.01 },  // Ω
            { "currentRating",  0.5  },  // A
        }),
        [] { return new FuseComponent(); });
}
