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

void AmmeterComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);
    drawCircleWithText(painter, QRectF(-18.0, -18.0, 36.0, 36.0), "A");
    drawLabel(painter);
}

VoltmeterComponent::VoltmeterComponent(QGraphicsItem* parent)
    : TwoTerminalElectricalComponent(parent)
{
    typeId = "ELEC_VOLTM";
    displayName = "Voltmeter";
}

void VoltmeterComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(componentPen(option->state & QStyle::State_Selected));
    painter->setBrush(componentBrush(destroyed));
    drawTerminals(painter);
    drawCircleWithText(painter, QRectF(-18.0, -18.0, 36.0, 36.0), "V");
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
}
