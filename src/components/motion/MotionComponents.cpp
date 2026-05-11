#include "components/motion/MotionComponents.h"

#include "components/ComponentRegistry.h"

#include <QPainter>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>

#include <cmath>

// ---------------------------------------------------------------------------
// Shared drawing helpers
// ---------------------------------------------------------------------------
namespace {

QPen motionPen(bool selected)
{
    return QPen(selected ? QColor(28, 100, 242) : QColor(35, 42, 50),
                selected ? 2.5 : 2.0);
}

// Draw a coil-spring zigzag from local point pA to local point pB.
// numCoils controls the number of full oscillation cycles.
void drawSpringCoil(QPainter* painter, const QPointF& pA, const QPointF& pB,
                    int numCoils = 6)
{
    const QPointF delta = pB - pA;
    const double len = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
    if (len < 4.0)
        return;

    const QPointF along = delta / len;                       // unit A→B
    const QPointF perp(-along.y(), along.x());               // perpendicular

    const double gapLen   = std::min(len * 0.12, 14.0);     // straight entry/exit
    const double amp      = std::min(10.0, len * 0.09);     // coil amplitude
    const double coilReg  = len - 2.0 * gapLen;             // spring coil region
    const double stepLen  = coilReg / (2.0 * numCoils);     // half-cycle length

    QPolygonF pts;
    pts << pA;
    pts << (pA + along * gapLen);

    for (int i = 0; i < 2 * numCoils; ++i) {
        const double t    = gapLen + (i + 1) * stepLen;
        const double sign = (i % 2 == 0) ? 1.0 : -1.0;
        pts << (pA + along * t + perp * (sign * amp));
    }

    pts << (pA + along * (len - gapLen));
    pts << pB;

    painter->drawPolyline(pts);
}

ComponentDescriptor motionDescriptor(
    const QString& typeId,
    const QString& displayName,
    const QString& description,
    const QMap<QString, QVariant>& defaultProperties = {})
{
    return ComponentDescriptor{
        typeId,
        displayName,
        "Motion/Mechanics",
        description,
        defaultProperties,
    };
}

} // namespace

// ===========================================================================
// BallComponent
// ===========================================================================

BallComponent::BallComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      centerPad{ QPointF(0.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "center", {}, false }
{
    typeId      = "MOT_BALL";
    displayName = "Ball";
    pads.append(&centerPad);
}

QRectF BallComponent::boundingRect() const
{
    const double r = properties.value("radius", 20.0).toDouble();
    // Extra 65 px covers the velocity arrow tip (r + 50 px) plus arrowhead.
    const double ext = r + 65.0;
    return QRectF(-ext, -ext, 2.0 * ext, 2.0 * ext);
}

void BallComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double r        = properties.value("radius", 20.0).toDouble();
    const double mass     = properties.value("mass",    1.0).toDouble();
    const bool   selected = option->state & QStyle::State_Selected;

    // Fill colour — brightens when moving.
    const bool moving = simState.value("speed", 0.0).toDouble() > 5.0;
    const QColor fill = destroyed   ? QColor(255, 235, 235)
                      : moving      ? QColor(160, 205, 255)
                                    : QColor(210, 230, 255);

    painter->setPen(motionPen(selected));
    painter->setBrush(fill);
    painter->drawEllipse(QPointF(0.0, 0.0), r, r);

    // Mass label centred inside the circle.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-r, -r, 2.0 * r, 2.0 * r), Qt::AlignCenter,
                      QString("%1 kg").arg(mass, 0, 'f', 1));

    // Velocity arrow rendered when simulation is running.
    if (simState.contains("vx")) {
        const double vx    = simState["vx"].toDouble();
        const double vy    = simState["vy"].toDouble();
        const double speed = std::hypot(vx, vy);
        if (speed > 2.0) {
            const double   arrowLen = std::min(speed * 0.04, 50.0);
            const QPointF  dir(vx / speed, vy / speed);
            const QPointF  tip = dir * (r + arrowLen);
            const QPointF  perp(-dir.y(), dir.x());
            painter->setPen(QPen(QColor(200, 60, 60), 2.0));
            painter->drawLine(QPointF(0.0, 0.0), tip);
            painter->drawLine(tip, tip - dir * 8.0 + perp * 4.0);
            painter->drawLine(tip, tip - dir * 8.0 - perp * 4.0);
        }
    }

    // Pad dot.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(0.0, 0.0), 3.5, 3.5);
}

// ===========================================================================
// BlockComponent
// ===========================================================================

BlockComponent::BlockComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(  0.0, -20.0), PadType::Bidirectional, DomainType::Mechanical, "top",    {}, false },
          ConnectionPad{ QPointF( 40.0,   0.0), PadType::Bidirectional, DomainType::Mechanical, "right",  {}, false },
          ConnectionPad{ QPointF(  0.0,  20.0), PadType::Bidirectional, DomainType::Mechanical, "bottom", {}, false },
          ConnectionPad{ QPointF(-40.0,   0.0), PadType::Bidirectional, DomainType::Mechanical, "left",   {}, false },
      }
{
    typeId      = "MOT_BLOCK";
    displayName = "Block";
    for (auto& p : padStorage)
        pads.append(&p);
}

QRectF BlockComponent::boundingRect() const
{
    const double hw = properties.value("halfW", 40.0).toDouble() + 6.0;
    const double hh = properties.value("halfH", 20.0).toDouble() + 6.0;
    return QRectF(-hw, -hh, 2.0 * hw, 2.0 * hh);
}

void BlockComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double hw    = properties.value("halfW",       40.0).toDouble();
    const double hh    = properties.value("halfH",       20.0).toDouble();
    const double mass  = properties.value("mass",         5.0).toDouble();
    const bool   fixed = properties.value("fixed",       false).toBool() || mass <= 0.0;
    const bool   sel   = option->state & QStyle::State_Selected;

    const QColor fill = destroyed ? QColor(255, 235, 235)
                      : fixed     ? QColor(160, 165, 170)
                                  : QColor(210, 235, 215);

    painter->setPen(QPen(sel ? QColor(28, 100, 242) : QColor(35, 42, 50),
                         sel ? 2.5 : 2.0,
                         fixed ? Qt::DashLine : Qt::SolidLine));
    painter->setBrush(fill);
    painter->drawRect(QRectF(-hw, -hh, 2.0 * hw, 2.0 * hh));

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-hw, -hh, 2.0 * hw, 2.0 * hh),
                      Qt::AlignCenter,
                      fixed ? "Wall" : QString("%1 kg").arg(mass, 0, 'f', 1));

    // Pad dots.
    painter->setBrush(QColor(28, 100, 242));
    painter->setPen(Qt::NoPen);
    for (const auto& pad : padStorage)
        painter->drawEllipse(pad.localPos, 3.5, 3.5);
}

// ===========================================================================
// SpringComponent
// ===========================================================================

SpringComponent::SpringComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-48.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "a", {}, false },
          ConnectionPad{ QPointF( 48.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "b", {}, false },
      }
{
    typeId      = "MOT_SPRING";
    displayName = "Spring";
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
}

QRectF SpringComponent::boundingRect() const
{
    // Large bounding rect so the spring is always rendered regardless of how
    // far apart the two connected bodies are in scene space.
    return QRectF(-2000.0, -2000.0, 4000.0, 4000.0);
}

void SpringComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option->state & QStyle::State_Selected;
    painter->setPen(QPen(selected ? QColor(28, 100, 242) : QColor(80, 120, 90), 2.0));
    painter->setBrush(Qt::NoBrush);

    // Endpoint A and B in local (item) coordinates.
    QPointF pA = padStorage[0].localPos;
    QPointF pB = padStorage[1].localPos;

    if (simState.value("hasLiveEndpoints", false).toBool()) {
        // Scene-space endpoints from solver — convert to item-local coordinates.
        pA = mapFromScene(QPointF(simState["endAx"].toDouble(), simState["endAy"].toDouble()));
        pB = mapFromScene(QPointF(simState["endBx"].toDouble(), simState["endBy"].toDouble()));
    }

    drawSpringCoil(painter, pA, pB);

    // Connection pad dots (always drawn at fixed pad positions for wire snap).
    painter->setBrush(QColor(80, 120, 90));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(padStorage[0].localPos, 3.5, 3.5);
    painter->drawEllipse(padStorage[1].localPos, 3.5, 3.5);

    // Static label when not in live-endpoint mode.
    if (!simState.value("hasLiveEndpoints", false).toBool()) {
        painter->setPen(QColor(35, 42, 50));
        painter->drawText(QRectF(-50.0, 14.0, 100.0, 16.0), Qt::AlignCenter, displayName);
    }
}

// ===========================================================================
// AnchorComponent
// ===========================================================================

AnchorComponent::AnchorComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      anchorPad{ QPointF(0.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "anchor", {}, false }
{
    typeId      = "MOT_ANCHOR";
    displayName = "Anchor";
    pads.append(&anchorPad);
}

QRectF AnchorComponent::boundingRect() const
{
    return QRectF(-22.0, -22.0, 44.0, 44.0);
}

void AnchorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool sel = option->state & QStyle::State_Selected;
    painter->setPen(QPen(sel ? QColor(28, 100, 242) : QColor(60, 70, 80), 2.0));

    // Cross-hair lines through the anchor point.
    painter->drawLine(QPointF(-16.0, 0.0), QPointF(16.0, 0.0));
    painter->drawLine(QPointF(0.0, -16.0), QPointF(0.0, 16.0));

    // Central filled circle.
    painter->setBrush(QColor(80, 90, 100));
    painter->drawEllipse(QPointF(0.0, 0.0), 6.0, 6.0);

    // Ground hatching below the anchor (indicates fixed to earth).
    painter->setPen(QPen(QColor(80, 90, 100), 1.5));
    painter->drawLine(QPointF(-16.0, 16.0), QPointF(16.0, 16.0));
    for (int i = -3; i <= 3; ++i) {
        painter->drawLine(QPointF(i * 5.0, 16.0), QPointF(i * 5.0 - 5.0, 22.0));
    }
}

// ===========================================================================
// PendulumComponent
// ===========================================================================

PendulumComponent::PendulumComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      pivotPad{ QPointF(0.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "pivot", {}, false }
{
    typeId      = "MOT_PENDULUM";
    displayName = "Pendulum";
    pads.append(&pivotPad);
}

QRectF PendulumComponent::boundingRect() const
{
    const double L = properties.value("length", 100.0).toDouble();
    const double r = properties.value("bobRadius", 12.0).toDouble();
    return QRectF(-(L + r + 4.0), -(r + 4.0), 2.0 * (L + r + 4.0), L + 2.0 * r + 8.0);
}

void PendulumComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool   selected = option->state & QStyle::State_Selected;
    const double L        = properties.value("length",    100.0).toDouble();
    const double r        = properties.value("bobRadius",  12.0).toDouble();
    const double ang0     = properties.value("angle",      30.0).toDouble();

    // Current arm angle in scene (from simState if running, else from property).
    const double angleDeg = simState.contains("angle")
        ? simState["angle"].toDouble() * 180.0 / M_PI
        : ang0;
    const double angleRad = angleDeg * M_PI / 180.0;

    // Bob position (local coordinates: pivot at origin, positive Y = down).
    const QPointF bob(L * std::sin(angleRad), L * std::cos(angleRad));

    // Arm.
    painter->setPen(motionPen(selected));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(0.0, 0.0), bob);

    // Pivot dot.
    painter->setBrush(QColor(60, 70, 80));
    painter->drawEllipse(QPointF(0.0, 0.0), 5.0, 5.0);

    // Bob.
    const QColor bobFill = destroyed ? QColor(255, 235, 235) : QColor(160, 200, 255);
    painter->setBrush(bobFill);
    painter->setPen(motionPen(selected));
    painter->drawEllipse(bob, r, r);

    // Angular velocity indicator.
    if (simState.contains("omega")) {
        const double omega = simState["omega"].toDouble();
        const QString omStr = QString("ω=%1").arg(omega, 0, 'f', 1);
        painter->setPen(QColor(35, 42, 50));
        painter->drawText(QRectF(bob.x() + r + 4.0, bob.y() - 10.0, 80.0, 20.0),
                          Qt::AlignLeft | Qt::AlignVCenter, omStr);
    }

    // Ground hatching at pivot.
    painter->setPen(QPen(QColor(80, 90, 100), 1.5));
    painter->drawLine(QPointF(-16.0, -6.0), QPointF(16.0, -6.0));
    for (int i = -3; i <= 3; ++i)
        painter->drawLine(QPointF(i * 5.0, -6.0), QPointF(i * 5.0 - 4.0, -12.0));
}

// ===========================================================================
// RampComponent
// ===========================================================================

RampComponent::RampComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "MOT_RAMP";
    displayName = "Ramp";
}

QRectF RampComponent::boundingRect() const
{
    const double L = properties.value("length", 140.0).toDouble();
    return QRectF(-L * 0.5 - 4.0, -20.0, L + 8.0, 50.0);
}

void RampComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double L   = properties.value("length", 140.0).toDouble();
    const bool   sel = option->state & QStyle::State_Selected;

    // Ramp surface: horizontal line (rotation of the item provides the slope).
    const QColor fill = destroyed ? QColor(255, 235, 235) : QColor(180, 190, 195);
    painter->setPen(QPen(sel ? QColor(28, 100, 242) : QColor(35, 42, 50),
                         sel ? 2.5 : 2.0));
    painter->setBrush(fill);

    const QPolygonF triangle {
        QPointF(-L * 0.5, 0.0),
        QPointF( L * 0.5, 0.0),
        QPointF(-L * 0.5, 32.0),
    };
    painter->drawPolygon(triangle);

    // Hatch fill indicating solid body.
    painter->setPen(QPen(QColor(120, 130, 140), 1.0));
    const int steps = static_cast<int>(L / 20.0);
    for (int i = 1; i < steps; ++i) {
        const double x = -L * 0.5 + i * (L / steps);
        const double y0 = 0.0;
        const double y1 = std::min(32.0, (x + L * 0.5) * 32.0 / L);
        painter->drawLine(QPointF(x, y0), QPointF(x, y1));
    }

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-L * 0.5, 34.0, L, 14.0), Qt::AlignCenter, displayName);
}

// ===========================================================================
// RopeComponent
// ===========================================================================

RopeComponent::RopeComponent(QGraphicsItem* parent)
    : BaseComponent(parent),
      padStorage{
          ConnectionPad{ QPointF(-48.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "a", {}, false },
          ConnectionPad{ QPointF( 48.0, 0.0), PadType::Bidirectional, DomainType::Mechanical, "b", {}, false },
      }
{
    typeId      = "MOT_ROPE";
    displayName = "Rope";
    pads.append(&padStorage[0]);
    pads.append(&padStorage[1]);
}

QRectF RopeComponent::boundingRect() const
{
    return QRectF(-2000.0, -2000.0, 4000.0, 4000.0);
}

void RopeComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool selected = option->state & QStyle::State_Selected;
    painter->setPen(QPen(selected ? QColor(28, 100, 242) : QColor(120, 80, 40), 2.5));
    painter->setBrush(Qt::NoBrush);

    QPointF pA = padStorage[0].localPos;
    QPointF pB = padStorage[1].localPos;

    if (simState.value("hasLiveEndpoints", false).toBool()) {
        pA = mapFromScene(QPointF(simState["endAx"].toDouble(), simState["endAy"].toDouble()));
        pB = mapFromScene(QPointF(simState["endBx"].toDouble(), simState["endBy"].toDouble()));
    }

    // Draw as a straight line (taut rope) or catenary sag if slack.
    const bool taut = simState.value("taut", true).toBool();
    if (taut || !simState.contains("hasLiveEndpoints")) {
        painter->drawLine(pA, pB);
    } else {
        // Simple sag approximation: draw a quadratic bezier with mid-point sagging.
        const QPointF mid = (pA + pB) * 0.5 + QPointF(0.0, 20.0);
        QPainterPath path;
        path.moveTo(pA);
        path.quadTo(mid, pB);
        painter->drawPath(path);
    }

    // Endpoint dots.
    painter->setBrush(QColor(120, 80, 40));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(padStorage[0].localPos, 3.5, 3.5);
    painter->drawEllipse(padStorage[1].localPos, 3.5, 3.5);

    if (!simState.value("hasLiveEndpoints", false).toBool()) {
        painter->setPen(QColor(35, 42, 50));
        painter->drawText(QRectF(-50.0, 14.0, 100.0, 16.0), Qt::AlignCenter, displayName);
    }
}

// ===========================================================================
// Registration
// ===========================================================================

void registerMotionComponents(ComponentRegistry& registry)
{
    registry.registerType(
        motionDescriptor("MOT_BALL", "Ball",
                         "Circular mass subject to gravity and collisions",
                         {
                             { "mass",       1.0  },
                             { "radius",    20.0  },
                             { "restitution", 0.8 },
                             { "velocityX",   0.0 },
                             { "velocityY",   0.0 },
                         }),
        [] { return new BallComponent(); });

    registry.registerType(
        motionDescriptor("MOT_BLOCK", "Block",
                         "Rigid rectangle wall or movable body (mass=0 → fixed wall)",
                         {
                             { "mass",        5.0  },
                             { "halfW",       40.0 },
                             { "halfH",       20.0 },
                             { "restitution",  0.5 },
                             { "fixed",       false },
                         }),
        [] { return new BlockComponent(); });

    registry.registerType(
        motionDescriptor("MOT_SPRING", "Spring",
                         "Hooke's-law spring connecting two mechanical pads",
                         {
                             { "stiffness",  200.0 },
                             { "restLength", 100.0 },
                             { "damping",      2.0 },
                         }),
        [] { return new SpringComponent(); });

    registry.registerType(
        motionDescriptor("MOT_ANCHOR", "Anchor",
                         "Fixed mount point for springs (zero mass, immovable)"),
        [] { return new AnchorComponent(); });

    registry.registerType(
        motionDescriptor("MOT_PENDULUM", "Pendulum",
                         "Simple gravity pendulum — set angle and length, watch it swing",
                         {
                             { "length",    120.0 },
                             { "angle",      30.0 },  // degrees from vertical
                             { "damping",     0.05 },
                             { "bobRadius",  12.0  },
                         }),
        [] { return new PendulumComponent(); });

    registry.registerType(
        motionDescriptor("MOT_RAMP", "Ramp",
                         "Inclined surface — balls roll and slide down it (rotate to set angle)",
                         {
                             { "length",      140.0 },
                             { "restitution",   0.5 },
                         }),
        [] { return new RampComponent(); });

    registry.registerType(
        motionDescriptor("MOT_ROPE", "Rope",
                         "Tension-only rope — pulls bodies together when taut, slack when loose",
                         {
                             { "length",     120.0 },
                             { "stiffness", 2000.0 },
                             { "damping",      5.0 },
                         }),
        [] { return new RopeComponent(); });
}
