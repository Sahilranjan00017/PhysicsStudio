#include "components/optics/OpticsComponents.h"

#include "components/ComponentRegistry.h"

#include <QColor>
#include <QPainter>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------
namespace {

// Map a wavelength in nm to a QColor.
// Default 550 nm → yellow-green, as typically expected.
QColor wavelengthToColor(double nm, double intensity = 1.0)
{
    double r, g, b;
    if      (nm < 380)              { r = 0.6; g = 0.0; b = 0.8; }
    else if (nm < 440) { double t = (nm - 380) / 60.0; r = 1.0 - t; g = 0.0; b = 1.0; }
    else if (nm < 490) { double t = (nm - 440) / 50.0; r = 0.0; g = t;       b = 1.0; }
    else if (nm < 510) { double t = (nm - 490) / 20.0; r = 0.0; g = 1.0;     b = 1.0 - t; }
    else if (nm < 580) { double t = (nm - 510) / 70.0; r = t;   g = 1.0;     b = 0.0; }
    else if (nm < 645) { double t = (nm - 580) / 65.0; r = 1.0; g = 1.0 - t; b = 0.0; }
    else if (nm <= 700)             { r = 1.0; g = 0.0; b = 0.0; }
    else                            { r = 0.5; g = 0.0; b = 0.0; }

    return QColor::fromRgbF(
        std::clamp(r * intensity, 0.0, 1.0),
        std::clamp(g * intensity, 0.0, 1.0),
        std::clamp(b * intensity, 0.0, 1.0),
        0.90);
}

QPen opticsPen(bool selected)
{
    return QPen(selected ? QColor(28, 100, 242) : QColor(35, 42, 50),
                selected ? 2.5 : 2.0);
}

ComponentDescriptor opticsDescriptor(
    const QString& typeId,
    const QString& displayName,
    const QString& description,
    const QMap<QString, QVariant>& defaultProps = {})
{
    return ComponentDescriptor{
        typeId, displayName, "Optics/Geometric", description, defaultProps};
}

} // namespace

// ===========================================================================
// LightSourceComponent
// ===========================================================================

LightSourceComponent::LightSourceComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_SRC";
    displayName = "Light Source";
}

QRectF LightSourceComponent::boundingRect() const
{
    return QRectF(-60.0, -50.0, 120.0, 110.0);
}

void LightSourceComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool   selected   = option->state & QStyle::State_Selected;
    const double angle      = properties.value("angle",      0.0).toDouble();
    const double spread     = properties.value("spread",     0.0).toDouble();
    const int    numRays    = std::clamp(properties.value("numRays", 1).toInt(), 1, 20);
    const double wavelength = properties.value("wavelength", 550.0).toDouble();

    // Lamp body — warm circle.
    painter->setPen(opticsPen(selected));
    painter->setBrush(QColor(255, 248, 200));
    painter->drawEllipse(QPointF(0.0, 0.0), 18.0, 18.0);

    // Ray preview arrows.
    const QColor rayColor = wavelengthToColor(wavelength);
    painter->setPen(QPen(rayColor, 1.8));
    painter->setBrush(rayColor);

    for (int i = 0; i < numRays; ++i) {
        double rayAngle = angle;
        if (numRays > 1)
            rayAngle = angle - spread * 0.5
                       + i * spread / static_cast<double>(numRays - 1);

        const double   rad(qDegreesToRadians(rayAngle));
        const QPointF  dir(std::cos(rad), std::sin(rad));
        const QPointF  perp(-dir.y(), dir.x());
        const QPointF  tipBase = dir * 18.0;
        const QPointF  tip     = dir * 44.0;

        painter->drawLine(tipBase, tip);
        // Arrowhead.
        const QPolygonF head = QPolygonF()
            << tip
            << tip - dir * 9.0 + perp * 4.5
            << tip - dir * 9.0 - perp * 4.5;
        painter->drawPolygon(head);
    }

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-50.0, 22.0, 100.0, 16.0), Qt::AlignCenter, "Light");
}

// ===========================================================================
// MirrorComponent
// ===========================================================================

MirrorComponent::MirrorComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_MIRROR";
    displayName = "Mirror";
}

QRectF MirrorComponent::boundingRect() const
{
    const double half = properties.value("length", 100.0).toDouble() * 0.5 + 10.0;
    return QRectF(-half, -20.0, 2.0 * half, 54.0);
}

void MirrorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double half     = properties.value("length", 100.0).toDouble() * 0.5;
    const bool   selected = option->state & QStyle::State_Selected;

    // Reflective surface — thick bright line.
    painter->setPen(QPen(selected ? QColor(28, 100, 242) : QColor(200, 215, 230), 4.0));
    painter->drawLine(QPointF(-half, 0.0), QPointF(half, 0.0));

    // Backing hatching (indicates which side is reflective).
    painter->setPen(QPen(QColor(120, 130, 140), 1.0));
    const int    hatchCount = static_cast<int>(half / 8.0);
    const double step       = (2.0 * half) / std::max(hatchCount, 1);
    for (int i = 0; i <= hatchCount; ++i) {
        const double x = -half + i * step;
        painter->drawLine(QPointF(x, 0.0), QPointF(x - 7.0, 10.0));
    }

    // Endpoint dots.
    painter->setBrush(selected ? QColor(28, 100, 242) : QColor(80, 90, 100));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(-half, 0.0), 3.5, 3.5);
    painter->drawEllipse(QPointF( half, 0.0), 3.5, 3.5);

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-half, 16.0, 2.0 * half, 16.0), Qt::AlignCenter, displayName);
}

// ===========================================================================
// LensComponent
// ===========================================================================

LensComponent::LensComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_LENS";
    displayName = "Lens";
}

QRectF LensComponent::boundingRect() const
{
    const double halfD = properties.value("diameter", 100.0).toDouble() * 0.5 + 8.0;
    return QRectF(-30.0, -halfD, 60.0, 2.0 * halfD + 36.0);
}

void LensComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfD      = properties.value("diameter",    100.0).toDouble() * 0.5;
    const double f          = properties.value("focalLength", 150.0).toDouble();
    const bool   converging = f > 0.0;
    const bool   selected   = option->state & QStyle::State_Selected;

    const QColor lensColor(180, 220, 245, 200);
    const QPen   borderPen(selected ? QColor(28, 100, 242) : QColor(60, 130, 180), 2.0);

    // Draw a simplified lens symbol: a rounded shape around the vertical surface.
    const double bulge = std::min(halfD * 0.35, 22.0);

    if (converging) {
        // Biconvex: convex on both sides.
        QPainterPath path;
        path.moveTo(0.0, -halfD);
        path.cubicTo( bulge, -halfD * 0.5,  bulge,  halfD * 0.5, 0.0,  halfD);
        path.cubicTo(-bulge,  halfD * 0.5, -bulge, -halfD * 0.5, 0.0, -halfD);
        painter->setPen(borderPen);
        painter->setBrush(lensColor);
        painter->drawPath(path);
    } else {
        // Biconcave: concave on both sides.
        QPainterPath path;
        path.moveTo(0.0, -halfD);
        path.cubicTo(-bulge, -halfD * 0.5, -bulge,  halfD * 0.5, 0.0,  halfD);
        path.cubicTo( bulge,  halfD * 0.5,  bulge, -halfD * 0.5, 0.0, -halfD);
        painter->setPen(borderPen);
        painter->setBrush(lensColor);
        painter->drawPath(path);
    }

    // Optical axis dashes.
    painter->setPen(QPen(QColor(100, 150, 200, 160), 1.0, Qt::DashLine));
    painter->drawLine(QPointF(-25.0, 0.0), QPointF(25.0, 0.0));

    // Focal-length label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-26.0, halfD + 6.0, 52.0, 16.0),
                      Qt::AlignCenter,
                      QString("f=%1").arg(f, 0, 'f', 0));
}

// ===========================================================================
// LightScreenComponent
// ===========================================================================

LightScreenComponent::LightScreenComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_SCREEN";
    displayName = "Screen";
}

QRectF LightScreenComponent::boundingRect() const
{
    const double halfL = properties.value("length", 120.0).toDouble() * 0.5 + 6.0;
    return QRectF(-18.0, -halfL, 52.0, 2.0 * halfL + 24.0);
}

void LightScreenComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfL    = properties.value("length", 120.0).toDouble() * 0.5;
    const bool   selected = option->state & QStyle::State_Selected;

    // Screen surface — white panel.
    painter->setPen(opticsPen(selected));
    painter->setBrush(QColor(255, 255, 255));
    painter->drawRect(QRectF(0.0, -halfL, 12.0, 2.0 * halfL));

    // Stand / support leg.
    painter->setPen(QPen(QColor(80, 90, 100), 2.0));
    painter->drawLine(QPointF(6.0,  halfL), QPointF(6.0, halfL + 14.0));
    painter->drawLine(QPointF(0.0,  halfL + 14.0), QPointF(12.0, halfL + 14.0));

    // Draw ray hit positions (scene coords → local coords via mapFromScene).
    const int hitCount = simState.value("hitCount", 0).toInt();
    if (hitCount > 0) {
        for (int i = 0; i < hitCount; ++i) {
            const double hx = simState.value(QString("hit%1x").arg(i), 0.0).toDouble();
            const double hy = simState.value(QString("hit%1y").arg(i), 0.0).toDouble();
            // Height on the screen in local y-coords.
            const QPointF localHit = mapFromScene(QPointF(hx, hy));
            const double  screenY  = std::clamp(localHit.y(), -halfL, halfL);
            painter->setBrush(QColor(255, 80, 30));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPointF(6.0, screenY), 3.5, 3.5);
        }
    }

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-10.0, halfL + 18.0, 40.0, 14.0),
                      Qt::AlignLeft, displayName);
}

// ===========================================================================
// Registration
// ===========================================================================

void registerOpticsComponents(ComponentRegistry& registry)
{
    registry.registerType(
        opticsDescriptor("OPT_SRC", "Light Source",
                         "Emits geometric light rays for ray-optics simulation",
                         {
                             { "angle",      0.0   },   // deg; 0=right, 90=down
                             { "spread",     0.0   },   // deg; 0=single ray
                             { "numRays",    1     },   // 1–20
                             { "intensity",  1.0   },
                             { "wavelength", 550.0 },   // nm
                             { "rayLength",  2000.0},   // px
                         }),
        [] { return new LightSourceComponent(); });

    registry.registerType(
        opticsDescriptor("OPT_MIRROR", "Mirror",
                         "Flat specular reflector — rotate to set angle",
                         {
                             { "length",       100.0 },
                             { "reflectivity",   0.95 },
                         }),
        [] { return new MirrorComponent(); });

    registry.registerType(
        opticsDescriptor("OPT_LENS", "Lens",
                         "Thin lens: positive focal length = converging, negative = diverging",
                         {
                             { "focalLength",   150.0 },
                             { "diameter",      100.0 },
                             { "transmittance",   0.97 },
                         }),
        [] { return new LensComponent(); });

    registry.registerType(
        opticsDescriptor("OPT_SCREEN", "Screen",
                         "Projection screen — shows where rays hit",
                         {
                             { "length", 120.0 },
                         }),
        [] { return new LightScreenComponent(); });
}
