#include "components/electronics/ElectronicsComponents.h"

#include "components/ComponentRegistry.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>

#include <cmath>

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
// InductorComponent
// ===========================================================================

InductorComponent::InductorComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_IND";
    displayName = "Inductor";
}

void InductorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(Qt::NoBrush);
    drawTerminals(painter);

    // Inductor symbol: 4 bumps (half-ellipses) along the wire axis.
    painter->setBrush(componentBrush(destroyed));
    const double cx0 = -27.0, step = 13.5;
    for (int i = 0; i < 4; ++i) {
        const double cx = cx0 + i * step;
        painter->drawArc(QRectF(cx, -9.0, step, 18.0), 0, 180 * 16);
    }

    // Live current reading.
    if (simState.contains("i_ind")) {
        const double I = simState["i_ind"].toDouble();
        painter->setPen(QColor(35, 42, 50));
        painter->setFont(QFont("Arial", 8));
        const QString txt = (qAbs(I) < 0.999) ? QString("%1 mA").arg(I*1000.0, 0, 'f', 1)
                                               : QString("%1 A").arg(I,         0, 'f', 3);
        painter->drawText(QRectF(-50.0, 14.0, 100.0, 16.0), Qt::AlignCenter, txt);
    } else {
        drawLabel(painter);
    }
}

// ===========================================================================
// TransformerComponent
// ===========================================================================

TransformerComponent::TransformerComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-60.0, -16.0), PadType::Bidirectional, DomainType::Electrical, "p1", {}, false },
          ConnectionPad{ QPointF(-60.0,  16.0), PadType::Bidirectional, DomainType::Electrical, "p2", {}, false },
          ConnectionPad{ QPointF( 60.0, -16.0), PadType::Bidirectional, DomainType::Electrical, "s1", {}, false },
          ConnectionPad{ QPointF( 60.0,  16.0), PadType::Bidirectional, DomainType::Electrical, "s2", {}, false },
      }
{
    typeId      = "ELEC_XFMR";
    displayName = "Transformer";
    for (auto& p : padStorage) pads.append(&p);
}

QRectF TransformerComponent::boundingRect() const
{
    return QRectF(-74.0, -40.0, 148.0, 92.0);
}

void TransformerComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(componentPen(sel));
    painter->setBrush(Qt::NoBrush);

    // Lead wires.
    painter->drawLine(QPointF(-60.0, -16.0), QPointF(-36.0, -16.0));
    painter->drawLine(QPointF(-60.0,  16.0), QPointF(-36.0,  16.0));
    painter->drawLine(QPointF( 36.0, -16.0), QPointF( 60.0, -16.0));
    painter->drawLine(QPointF( 36.0,  16.0), QPointF( 60.0,  16.0));

    // Primary coil bumps (left side).
    for (int i = 0; i < 3; ++i)
        painter->drawArc(QRectF(-36.0 + i * 12.0, -22.0, 12.0, 12.0), 0, 180*16);
    for (int i = 0; i < 3; ++i)
        painter->drawArc(QRectF(-36.0 + i * 12.0,  10.0, 12.0, 12.0), 0, 180*16);

    // Secondary coil bumps (right side, mirrored).
    for (int i = 0; i < 3; ++i)
        painter->drawArc(QRectF(0.0 + i * 12.0, -22.0, 12.0, 12.0), 0, 180*16);
    for (int i = 0; i < 3; ++i)
        painter->drawArc(QRectF(0.0 + i * 12.0,  10.0, 12.0, 12.0), 0, 180*16);

    // Coupling core lines.
    painter->setPen(QPen(sel ? QColor(28,100,242) : QColor(35,42,50), 2.5));
    painter->drawLine(QPointF(-2.0, -26.0), QPointF(-2.0, 26.0));
    painter->drawLine(QPointF( 2.0, -26.0), QPointF( 2.0, 26.0));

    // Pad dots.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (const auto& p : padStorage)
        painter->drawEllipse(p.localPos, 3.5, 3.5);

    // Ratio label.
    const double n = properties.value("ratio", 2.0).toDouble();
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-50.0, 32.0, 100.0, 16.0), Qt::AlignCenter,
                      QString("1 : %1").arg(n, 0, 'f', 1));
}

// ===========================================================================
// NPNTransistorComponent
// ===========================================================================

NPNTransistorComponent::NPNTransistorComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-36.0,  0.0), PadType::Bidirectional, DomainType::Electrical, "B", {}, false },
          ConnectionPad{ QPointF(  0.0, -36.0), PadType::Bidirectional, DomainType::Electrical, "C", {}, false },
          ConnectionPad{ QPointF(  0.0,  36.0), PadType::Bidirectional, DomainType::Electrical, "E", {}, false },
      }
{
    typeId      = "ELEC_NPN";
    displayName = "NPN BJT";
    for (auto& p : padStorage) pads.append(&p);
}

QRectF NPNTransistorComponent::boundingRect() const
{
    return QRectF(-44.0, -44.0, 96.0, 100.0);
}

void NPNTransistorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool   sel    = option->state & QStyle::State_Selected;
    const QString region = simState.value("region", "off").toString();
    const QColor bodyFill = (region == "active") ? QColor(200, 230, 255)
                          : destroyed             ? QColor(255, 235, 235)
                                                  : QColor(230, 230, 230);

    painter->setPen(componentPen(sel));

    // Outer circle body.
    painter->setBrush(bodyFill);
    painter->drawEllipse(QPointF(8.0, 0.0), 30.0, 30.0);

    // Base lead (horizontal).
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(-36.0, 0.0), QPointF(-8.0, 0.0));
    // Base vertical bar inside.
    painter->setPen(QPen(sel ? QColor(28,100,242) : QColor(35,42,50), 3.0));
    painter->drawLine(QPointF(-8.0, -18.0), QPointF(-8.0, 18.0));

    // Collector lead (upper right diagonal).
    painter->setPen(componentPen(sel));
    painter->drawLine(QPointF(-8.0, -10.0), QPointF( 8.0, -22.0));
    painter->drawLine(QPointF( 8.0, -22.0), QPointF( 0.0, -36.0));

    // Emitter lead (lower right diagonal) with arrow.
    painter->drawLine(QPointF(-8.0, 10.0), QPointF( 8.0, 22.0));
    painter->drawLine(QPointF( 8.0, 22.0), QPointF( 0.0, 36.0));

    // Emitter arrowhead (NPN: pointing outward from transistor body).
    const QPointF tip( 8.0, 22.0);
    const QPointF dir = QPointF( 8.0, 22.0) - QPointF(-8.0, 10.0);
    const double len  = std::hypot(dir.x(), dir.y());
    const QPointF d   = dir / len;
    const QPointF perp(-d.y(), d.x());
    painter->setBrush(sel ? QColor(28,100,242) : QColor(35,42,50));
    painter->drawPolygon(QPolygonF() << tip
                                     << tip - d*8.0 + perp*4.0
                                     << tip - d*8.0 - perp*4.0);

    // Labels for terminals.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (const auto& p : padStorage)
        painter->drawEllipse(p.localPos, 3.5, 3.5);

    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-50.0, 42.0, 100.0, 14.0), Qt::AlignCenter,
                      region == "active" ? "NPN [active]" : "NPN [off]");
}

// ===========================================================================
// LogicGateComponent
// ===========================================================================

LogicGateComponent::LogicGateComponent(const QString& gateTypeId, QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-36.0, -12.0), PadType::Bidirectional, DomainType::Electrical, "in1", {}, false },
          ConnectionPad{ QPointF(-36.0,  12.0), PadType::Bidirectional, DomainType::Electrical, "in2", {}, false },
          ConnectionPad{ QPointF( 36.0,   0.0), PadType::Bidirectional, DomainType::Electrical, "out",  {}, false },
      }
{
    typeId = gateTypeId;
    if (typeId == "ELEC_NOT") {
        displayName   = "NOT Gate";
        m_numInputs   = 1;
        // Reposition single input to centre, remove in2.
        padStorage[0].localPos = QPointF(-36.0, 0.0);
        padStorage[0].padId    = "in";
        pads.append(&padStorage[0]);
        pads.append(&padStorage[2]);  // out only
    } else {
        if      (typeId == "ELEC_AND")  displayName = "AND Gate";
        else if (typeId == "ELEC_OR")   displayName = "OR Gate";
        else if (typeId == "ELEC_NAND") displayName = "NAND Gate";
        else if (typeId == "ELEC_NOR")  displayName = "NOR Gate";
        else if (typeId == "ELEC_XOR")  displayName = "XOR Gate";
        m_numInputs = 2;
        pads.append(&padStorage[0]);
        pads.append(&padStorage[1]);
        pads.append(&padStorage[2]);
    }
}

QRectF LogicGateComponent::boundingRect() const
{
    return QRectF(-44.0, -28.0, 96.0, 68.0);
}

void LogicGateComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(componentPen(sel));

    const double Vh   = properties.value("Vhigh", 5.0).toDouble();
    const double Vthr = properties.value("Vthreshold", 2.5).toDouble();
    const double outV = simState.value("outV", 0.0).toDouble();

    const bool outHigh = (outV > Vthr);
    const QColor bodyFill = destroyed ? QColor(255,235,235)
                          : outHigh   ? QColor(200,255,200)
                                      : QColor(230,230,230);

    painter->setBrush(bodyFill);

    // Helper: draw AND body.
    auto drawAndBody = [&]() {
        QPainterPath path;
        path.moveTo(-18.0, -20.0);
        path.lineTo( 4.0,  -20.0);
        path.cubicTo(28.0, -20.0, 28.0, 20.0, 4.0, 20.0);
        path.lineTo(-18.0,  20.0);
        path.closeSubpath();
        painter->drawPath(path);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(-36.0, -12.0), QPointF(-18.0, -12.0));
        painter->drawLine(QPointF(-36.0,  12.0), QPointF(-18.0,  12.0));
        painter->drawLine(QPointF( 28.0,   0.0), QPointF( 36.0,   0.0));
    };
    // Helper: draw OR body.
    auto drawOrBody = [&]() {
        QPainterPath path;
        path.moveTo(-18.0, -20.0);
        path.cubicTo(-10.0, -20.0, 10.0, -18.0, 28.0, 0.0);
        path.cubicTo(10.0, 18.0, -10.0, 20.0, -18.0, 20.0);
        path.cubicTo(-8.0, 10.0, -8.0, -10.0, -18.0, -20.0);
        painter->drawPath(path);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(-36.0, -12.0), QPointF(-14.0, -12.0));
        painter->drawLine(QPointF(-36.0,  12.0), QPointF(-14.0,  12.0));
        painter->drawLine(QPointF( 28.0,   0.0), QPointF( 36.0,   0.0));
    };
    // Helper: bubble at output (for NAND/NOR).
    auto drawOutputBubble = [&]() {
        painter->setBrush(bodyFill);
        painter->drawEllipse(QPointF(34.0, 0.0), 6.0, 6.0);
        painter->setBrush(Qt::NoBrush);
        // Shorten output lead to meet bubble; add lead after.
        painter->drawLine(QPointF(40.0, 0.0), QPointF(36.0 + 6.0, 0.0));
    };

    if (typeId == "ELEC_AND") {
        drawAndBody();
    } else if (typeId == "ELEC_NAND") {
        // AND body + output bubble.
        drawAndBody();
        painter->setBrush(bodyFill);
        painter->drawEllipse(QPointF(34.0, 0.0), 6.0, 6.0);
    } else if (typeId == "ELEC_OR") {
        drawOrBody();
    } else if (typeId == "ELEC_NOR") {
        // OR body + output bubble.
        drawOrBody();
        painter->setBrush(bodyFill);
        painter->drawEllipse(QPointF(34.0, 0.0), 6.0, 6.0);
    } else if (typeId == "ELEC_XOR") {
        // OR body + extra curved line at input side.
        drawOrBody();
        QPainterPath extra;
        extra.moveTo(-22.0, -20.0);
        extra.cubicTo(-12.0, -10.0, -12.0, 10.0, -22.0, 20.0);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(extra);
    } else { // NOT
        // Triangle with bubble at output.
        const QPolygonF tri {
            QPointF(-18.0, -18.0), QPointF(-18.0, 18.0), QPointF(22.0, 0.0)
        };
        painter->drawPolygon(tri);
        painter->setBrush(bodyFill);
        painter->drawEllipse(QPointF(28.0, 0.0), 6.0, 6.0);
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPointF(-36.0, 0.0), QPointF(-18.0, 0.0));
        painter->drawLine(QPointF(34.0,  0.0), QPointF( 36.0, 0.0));
    }
    Q_UNUSED(drawOutputBubble)

    // Pad dots.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (auto* pad : pads)
        painter->drawEllipse(pad->localPos, 3.5, 3.5);

    // Output voltage label.
    painter->setPen(QColor(35, 42, 50));
    const QString outLabel = outHigh
        ? QString("→ %1V (H)").arg(Vh, 0, 'f', 1)
        : "→ 0V (L)";
    painter->drawText(QRectF(-40.0, 22.0, 90.0, 14.0), Qt::AlignCenter, outLabel);
}

// ===========================================================================
// PNPTransistorComponent
// ===========================================================================

PNPTransistorComponent::PNPTransistorComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-36.0,  0.0), PadType::Bidirectional, DomainType::Electrical, "b", {}, false },
          ConnectionPad{ QPointF(  4.0,-36.0), PadType::Bidirectional, DomainType::Electrical, "c", {}, false },
          ConnectionPad{ QPointF(  4.0, 36.0), PadType::Bidirectional, DomainType::Electrical, "e", {}, false },
      }
{
    typeId      = "ELEC_PNP";
    displayName = "PNP Transistor";
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
    pads.append(&padStorage[2]);
}

QRectF PNPTransistorComponent::boundingRect() const
{
    return QRectF(-44.0, -44.0, 88.0, 100.0);
}

void PNPTransistorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(componentPen(sel));

    const QString region = simState.value("region", "off").toString();
    const QColor bodyColor = (region == "active") ? QColor(230, 250, 220) : QColor(245, 245, 245);

    // Transistor outline circle.
    painter->setBrush(bodyColor);
    painter->drawEllipse(QPointF(0.0, 0.0), 28.0, 28.0);

    // Base lead (left).
    painter->drawLine(QPointF(-36.0, 0.0), QPointF(-16.0, 0.0));
    // Vertical bar.
    painter->drawLine(QPointF(-16.0, -18.0), QPointF(-16.0, 18.0));
    // Collector (top).
    painter->drawLine(QPointF(-16.0, -12.0), QPointF(4.0, -28.0));
    painter->drawLine(QPointF(4.0, -28.0), QPointF(4.0, -36.0));
    // Emitter with arrow pointing INTO base (PNP direction: current flows E→base).
    painter->drawLine(QPointF(-16.0,  12.0), QPointF(4.0,  28.0));
    painter->drawLine(QPointF(4.0,   28.0), QPointF(4.0,  36.0));

    // PNP emitter arrow: points from emitter body TOWARD base (inward arrow).
    {
        const QPointF tip(-16.0, 12.0);
        const QPointF dir = QPointF(4.0, 28.0) - tip;
        const double  len  = std::sqrt(dir.x()*dir.x() + dir.y()*dir.y());
        const QPointF unit = dir / len;
        const QPointF perp(-unit.y(), unit.x());
        const QPointF base = tip + unit * 10.0;
        const QPolygonF arrow{ tip, base + perp * 5.0, base - perp * 5.0 };
        painter->setBrush(componentPen(sel).color());
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(arrow);
        painter->setPen(componentPen(sel));
    }

    // Pad dots.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (auto* p : pads)
        painter->drawEllipse(p->localPos, 3.5, 3.5);

    // Labels.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-42.0,  -8.0, 16.0, 14.0), Qt::AlignCenter, "B");
    painter->drawText(QRectF(  8.0, -42.0, 20.0, 14.0), Qt::AlignCenter, "C");
    painter->drawText(QRectF(  8.0,  28.0, 20.0, 14.0), Qt::AlignCenter, "E");

    if (simState.contains("iC")) {
        const double iC = simState["iC"].toDouble();
        painter->drawText(QRectF(-28.0, 30.0, 60.0, 14.0), Qt::AlignCenter,
                          QString("IC=%1mA").arg(iC * 1000.0, 0, 'f', 2));
    }
}

// ===========================================================================
// OpAmpComponent
// ===========================================================================

OpAmpComponent::OpAmpComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-48.0, -16.0), PadType::Bidirectional, DomainType::Electrical, "in+", {}, false },
          ConnectionPad{ QPointF(-48.0,  16.0), PadType::Bidirectional, DomainType::Electrical, "in-", {}, false },
          ConnectionPad{ QPointF( 48.0,   0.0), PadType::Bidirectional, DomainType::Electrical, "out", {}, false },
      }
{
    typeId      = "ELEC_OPAMP";
    displayName = "Op-Amp";
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
    pads.append(&padStorage[2]);
}

QRectF OpAmpComponent::boundingRect() const
{
    return QRectF(-56.0, -42.0, 112.0, 96.0);
}

void OpAmpComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(componentPen(sel));

    const double outV = simState.value("outV", 0.0).toDouble();
    const double Vpos = properties.value("supplyPos",  15.0).toDouble();
    const double satFrac = std::abs(outV) > 0.01 ? std::abs(outV) / Vpos : 0.0;
    const QColor fillColor = QColor(
        200 + static_cast<int>(55.0 * (1.0 - satFrac)),
        220,
        255);

    // Triangle body.
    const QPolygonF tri {
        QPointF(-32.0, -32.0),
        QPointF(-32.0,  32.0),
        QPointF( 36.0,   0.0)
    };
    painter->setBrush(fillColor);
    painter->drawPolygon(tri);

    // Input leads.
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(-48.0, -16.0), QPointF(-32.0, -16.0));
    painter->drawLine(QPointF(-48.0,  16.0), QPointF(-32.0,  16.0));
    painter->drawLine(QPointF( 36.0,   0.0), QPointF( 48.0,   0.0));

    // +/− symbols inside.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-30.0, -24.0, 20.0, 14.0), Qt::AlignCenter, "+");
    painter->drawText(QRectF(-30.0,  10.0, 20.0, 14.0), Qt::AlignCenter, "−");

    // Pad dots.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (auto* p : pads)
        painter->drawEllipse(p->localPos, 3.5, 3.5);

    // Output voltage.
    painter->setPen(QColor(35, 42, 50));
    if (simState.contains("outV")) {
        painter->drawText(QRectF(-56.0, 34.0, 112.0, 14.0), Qt::AlignCenter,
                          QString("Vout = %1 V").arg(outV, 0, 'f', 2));
    }
}

// ===========================================================================
// ZenerDiodeComponent
// ===========================================================================

ZenerDiodeComponent::ZenerDiodeComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId      = "ELEC_ZENER";
    displayName = "Zener Diode";
}

void ZenerDiodeComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(componentPen(sel));

    // Determine state from previous tick.
    const double V_diff = simState.value("voltageDiff", 0.0).toDouble();
    const double Vf     = properties.value("forwardVoltage",  0.7).toDouble();
    const double Vz     = properties.value("zenerVoltage",    5.1).toDouble();
    const bool fwdOn    = (V_diff >  Vf * 0.5);
    const bool zenOn    = (V_diff < -Vz * 0.5);
    const QColor fillColor = fwdOn ? QColor(255, 220, 180)
                           : zenOn ? QColor(180, 220, 255)
                                   : QColor(255, 255, 255);

    drawTerminals(painter);
    painter->setBrush(fillColor);

    // Diode triangle (anode left → cathode right).
    const QPolygonF tri { QPointF(-20.0, -16.0), QPointF(-20.0, 16.0), QPointF(20.0, 0.0) };
    painter->drawPolygon(tri);

    // Zener cathode bar with bent ends (Z-shape).
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(20.0, -16.0), QPointF(20.0,  16.0));
    painter->drawLine(QPointF(20.0, -16.0), QPointF(28.0, -22.0)); // upper bent end
    painter->drawLine(QPointF(20.0,  16.0), QPointF(12.0,  22.0)); // lower bent end

    // Labels.
    painter->setPen(QColor(35, 42, 50));
    const double Vz_val = properties.value("zenerVoltage", 5.1).toDouble();
    painter->drawText(QRectF(-50.0, 18.0, 100.0, 14.0), Qt::AlignCenter,
                      QString("Vz = %1 V").arg(Vz_val, 0, 'f', 1));
    drawLabel(painter);
}

// ===========================================================================
// PotentiometerComponent
// ===========================================================================

PotentiometerComponent::PotentiometerComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-52.0,  0.0), PadType::Bidirectional, DomainType::Electrical, "a",     {}, false },
          ConnectionPad{ QPointF(  0.0,-36.0), PadType::Bidirectional, DomainType::Electrical, "wiper", {}, false },
          ConnectionPad{ QPointF( 52.0,  0.0), PadType::Bidirectional, DomainType::Electrical, "b",     {}, false },
      }
{
    typeId      = "ELEC_POT";
    displayName = "Potentiometer";
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
    pads.append(&padStorage[2]);
}

QRectF PotentiometerComponent::boundingRect() const
{
    return QRectF(-60.0, -48.0, 120.0, 80.0);
}

void PotentiometerComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(componentPen(sel));
    painter->setBrush(QColor(255, 255, 255));

    // Resistor body (rectangle).
    painter->drawRect(QRectF(-28.0, -12.0, 56.0, 24.0));

    // Terminal leads.
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(-52.0, 0.0), QPointF(-28.0, 0.0));
    painter->drawLine(QPointF( 28.0, 0.0), QPointF( 52.0, 0.0));

    // Wiper arrow: from top, pointing down to indicate position on resistor.
    const double pos = std::clamp(properties.value("position", 0.5).toDouble(), 0.001, 0.999);
    const double wiperX = -28.0 + pos * 56.0;
    painter->drawLine(QPointF(wiperX, -36.0), QPointF(wiperX, -12.0));
    // Arrowhead.
    const QPolygonF arrow {
        QPointF(wiperX,       -12.0),
        QPointF(wiperX - 5.0, -20.0),
        QPointF(wiperX + 5.0, -20.0)
    };
    painter->setBrush(componentPen(sel).color());
    painter->setPen(Qt::NoPen);
    painter->drawPolygon(arrow);
    painter->setPen(componentPen(sel));
    painter->setBrush(Qt::NoBrush);

    // Resistor hatch pattern.
    painter->setPen(QPen(QColor(120, 120, 120), 0.8));
    for (int i = -24; i <= 24; i += 8)
        painter->drawLine(QPointF(i, -12.0), QPointF(i + 4, 12.0));

    // Pad dots.
    painter->setPen(componentPen(sel));
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (auto* p : pads)
        painter->drawEllipse(p->localPos, 3.5, 3.5);

    // Labels.
    painter->setPen(QColor(35, 42, 50));
    const double R = properties.value("resistance", 10000.0).toDouble();
    painter->drawText(QRectF(-52.0, 22.0, 104.0, 14.0), Qt::AlignCenter,
                      QString("R = %1 Ω  pos = %2")
                          .arg(R, 0, 'f', 0)
                          .arg(pos, 0, 'f', 2));
}

// ===========================================================================
// OscilloscopeComponent
// ===========================================================================

OscilloscopeComponent::OscilloscopeComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-52.0, 0.0), PadType::Bidirectional, DomainType::Electrical, "ch+", {}, false },
          ConnectionPad{ QPointF( 52.0, 0.0), PadType::Bidirectional, DomainType::Electrical, "ch-", {}, false },
      }
{
    typeId      = "ELEC_SCOPE";
    displayName = "Oscilloscope";
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
}

QRectF OscilloscopeComponent::boundingRect() const
{
    return QRectF(-60.0, -46.0, 120.0, 108.0);
}

void OscilloscopeComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = option->state & QStyle::State_Selected;

    // Scope cabinet.
    painter->setPen(componentPen(sel));
    painter->setBrush(QColor(40, 45, 55));
    painter->drawRoundedRect(QRectF(-48.0, -40.0, 96.0, 76.0), 5.0, 5.0);

    // Screen.
    painter->setBrush(QColor(10, 20, 10));
    painter->setPen(Qt::NoPen);
    painter->drawRect(QRectF(-38.0, -32.0, 76.0, 50.0));

    // Grid lines on screen.
    painter->setPen(QPen(QColor(0, 80, 0), 0.5));
    for (int gx = -19; gx <= 38; gx += 19)
        painter->drawLine(QPointF(gx, -32.0), QPointF(gx, 18.0));
    for (int gy = -32; gy <= 18; gy += 25)
        painter->drawLine(QPointF(-38.0, gy), QPointF(38.0, gy));

    // Waveform trace (live if available, else static demo sine).
    painter->setPen(QPen(QColor(0, 230, 60), 1.5));
    if (simState.contains("voltageDiff")) {
        const double V    = simState["voltageDiff"].toDouble();
        const double Vmax = std::max(std::abs(V), 1.0);
        // Draw last-value as a horizontal line at that amplitude.
        const double screenY = -7.0 - (V / Vmax) * 20.0;
        painter->drawLine(QPointF(-38.0, screenY), QPointF(38.0, screenY));
    } else {
        // Static sine demo.
        QPainterPath wave;
        for (int px = -38; px <= 38; px += 2) {
            const double angle = (px + 38.0) / 76.0 * 2.0 * M_PI;
            const double y = -7.0 - std::sin(angle) * 18.0;
            if (px == -38) wave.moveTo(QPointF(px, y));
            else           wave.lineTo(QPointF(px, y));
        }
        painter->drawPath(wave);
    }

    // Lead wires.
    painter->setPen(componentPen(sel));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(-52.0, 0.0), QPointF(-48.0, 0.0));
    painter->drawLine(QPointF( 48.0, 0.0), QPointF( 52.0, 0.0));

    // Pad dots + CH+/CH- labels.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(padStorage[0].localPos, 3.5, 3.5);
    painter->drawEllipse(padStorage[1].localPos, 3.5, 3.5);

    painter->setPen(QColor(200, 200, 200));
    painter->drawText(QRectF(-60.0, 34.0, 30.0, 14.0), Qt::AlignCenter, "CH+");
    painter->drawText(QRectF( 28.0, 34.0, 36.0, 14.0), Qt::AlignCenter, "CH-");

    // Voltage reading.
    if (simState.contains("voltageDiff")) {
        const double V = simState["voltageDiff"].toDouble();
        painter->setPen(QColor(0, 230, 60));
        painter->drawText(QRectF(-38.0, 20.0, 76.0, 14.0), Qt::AlignCenter,
                          QString("%1 V").arg(V, 0, 'f', 3));
    }
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

    registry.registerType(
        descriptor("ELEC_IND", "Inductor",
                   "Stores energy in magnetic field — enables LC/RL transient circuits", {
            { "inductance", 0.01 },  // H (10 mH)
        }),
        [] { return new InductorComponent(); });

    registry.registerType(
        descriptor("ELEC_XFMR", "Transformer",
                   "Ideal 1:n transformer — secondary voltage = n × primary voltage", {
            { "ratio", 2.0 },   // turns ratio Ns/Np
        }),
        [] { return new TransformerComponent(); });

    registry.registerType(
        descriptor("ELEC_NPN", "NPN Transistor",
                   "BJT transistor: IC = hFE·IB current amplifier / electronic switch", {
            { "hFE",  100.0 },   // current gain (β)
            { "Vth",    0.6 },   // base-emitter threshold (V)
            { "Rbe", 1000.0 },   // base-emitter resistance (Ω)
        }),
        [] { return new NPNTransistorComponent(); });

    registry.registerType(
        descriptor("ELEC_PNP", "PNP Transistor",
                   "Complementary BJT: active when V_EB > Vth; IC = hFE·IB (emitter→collector)", {
            { "hFE",  100.0 },
            { "Vth",    0.6 },
            { "Rbe", 1000.0 },
        }),
        [] { return new PNPTransistorComponent(); });

    registry.registerType(
        descriptor("ELEC_OPAMP", "Op-Amp",
                   "Ideal op-amp: Vout = A·(V+−V−) clamped to supply rails (use in negative feedback)", {
            { "gain",       100000.0 },  // open-loop gain A
            { "supplyPos",      15.0 },  // positive rail (V)
            { "supplyNeg",     -15.0 },  // negative rail (V)
        }),
        [] { return new OpAmpComponent(); });

    registry.registerType(
        descriptor("ELEC_ZENER", "Zener Diode",
                   "Voltage regulator: forward diode + reverse breakdown clamping at Vz", {
            { "forwardVoltage", 0.7 },
            { "zenerVoltage",   5.1 },
            { "onResistance",   0.5 },
        }),
        [] { return new ZenerDiodeComponent(); });

    registry.registerType(
        descriptor("ELEC_POT", "Potentiometer",
                   "Three-terminal variable resistor — wiper divides total resistance by position", {
            { "resistance", 10000.0 },  // total Ω
            { "position",      0.5  },  // wiper fraction (0–1)
        }),
        [] { return new PotentiometerComponent(); });

    registry.registerType(
        descriptor("ELEC_AND", "AND Gate",
                   "Digital AND: output HIGH only when both inputs are HIGH", {
            { "Vhigh",       5.0 },
            { "Vlow",        0.0 },
            { "Vthreshold",  2.5 },
        }),
        [] { return new LogicGateComponent("ELEC_AND"); });

    registry.registerType(
        descriptor("ELEC_OR", "OR Gate",
                   "Digital OR: output HIGH when at least one input is HIGH", {
            { "Vhigh",       5.0 },
            { "Vlow",        0.0 },
            { "Vthreshold",  2.5 },
        }),
        [] { return new LogicGateComponent("ELEC_OR"); });

    registry.registerType(
        descriptor("ELEC_NOT", "NOT Gate",
                   "Digital inverter: output is the logical complement of the input", {
            { "Vhigh",       5.0 },
            { "Vlow",        0.0 },
            { "Vthreshold",  2.5 },
        }),
        [] { return new LogicGateComponent("ELEC_NOT"); });

    registry.registerType(
        descriptor("ELEC_NAND", "NAND Gate",
                   "NOT-AND: output LOW only when both inputs are HIGH", {
            { "Vhigh",       5.0 },
            { "Vlow",        0.0 },
            { "Vthreshold",  2.5 },
        }),
        [] { return new LogicGateComponent("ELEC_NAND"); });

    registry.registerType(
        descriptor("ELEC_NOR", "NOR Gate",
                   "NOT-OR: output HIGH only when both inputs are LOW", {
            { "Vhigh",       5.0 },
            { "Vlow",        0.0 },
            { "Vthreshold",  2.5 },
        }),
        [] { return new LogicGateComponent("ELEC_NOR"); });

    registry.registerType(
        descriptor("ELEC_XOR", "XOR Gate",
                   "Exclusive-OR: output HIGH when inputs differ", {
            { "Vhigh",       5.0 },
            { "Vlow",        0.0 },
            { "Vthreshold",  2.5 },
        }),
        [] { return new LogicGateComponent("ELEC_XOR"); });

    registry.registerType(
        descriptor("ELEC_SCOPE", "Oscilloscope",
                   "Measures and displays voltage waveform between two nodes", {
            { "inputResistance", 1000000000.0 },
        }),
        [] { return new OscilloscopeComponent(); });
}
