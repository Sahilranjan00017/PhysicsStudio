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
// PrismComponent
// ===========================================================================

PrismComponent::PrismComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_PRISM";
    displayName = "Prism";
}

QRectF PrismComponent::boundingRect() const
{
    const double s = properties.value("sideLength", 100.0).toDouble();
    const double h = s * std::sqrt(3.0) / 2.0;
    return QRectF(-s * 0.5 - 6.0, -h * 0.5 - 6.0, s + 12.0, h + 20.0);
}

void PrismComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double s       = properties.value("sideLength", 100.0).toDouble();
    const double h       = s * std::sqrt(3.0) / 2.0;
    const bool   selected = option->state & QStyle::State_Selected;

    // Equilateral triangle (apex up).
    const QPolygonF tri {
        QPointF(0.0,       -h * 0.5),   // apex
        QPointF(-s * 0.5,  h * 0.5),   // bottom-left
        QPointF( s * 0.5,  h * 0.5),   // bottom-right
    };

    // Glass-blue fill with gradient to hint at refraction.
    QLinearGradient grad(-s * 0.5, 0.0, s * 0.5, 0.0);
    grad.setColorAt(0.0, QColor(160, 200, 255, 180));
    grad.setColorAt(1.0, QColor(220, 240, 255, 180));

    painter->setPen(opticsPen(selected));
    painter->setBrush(grad);
    painter->drawPolygon(tri);

    // Rainbow spectrum fan hinting at dispersion (three coloured lines).
    if (!selected) {
        const QPointF base(s * 0.5, h * 0.5);
        for (int i = 0; i < 4; ++i) {
            const double angle = (15.0 + i * 10.0) * M_PI / 180.0;
            const QColor cols[] = { QColor(255,60,60), QColor(255,200,0),
                                    QColor(0,200,60), QColor(80,80,255) };
            painter->setPen(QPen(cols[i], 1.5));
            painter->drawLine(base, base + QPointF(std::cos(angle)*28.0,
                                                   std::sin(angle)*28.0));
        }
    }

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-s * 0.5, h * 0.5 + 4.0, s, 14.0),
                      Qt::AlignCenter, displayName);
}

// ===========================================================================
// FilterComponent
// ===========================================================================

FilterComponent::FilterComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_FILTER";
    displayName = "Filter";
}

QRectF FilterComponent::boundingRect() const
{
    const double halfL = properties.value("length", 80.0).toDouble() * 0.5 + 6.0;
    return QRectF(-18.0, -halfL, 48.0, 2.0 * halfL + 20.0);
}

void FilterComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfL  = properties.value("length", 80.0).toDouble() * 0.5;
    const double cwl    = properties.value("centerWavelength", 550.0).toDouble();
    const double trans  = properties.value("transmittance", 0.9).toDouble();
    const bool   selected = option->state & QStyle::State_Selected;

    const QColor filterCol = wavelengthToColor(cwl, trans);

    painter->setPen(opticsPen(selected));
    painter->setBrush(filterCol);
    painter->drawRect(QRectF(0.0, -halfL, 12.0, 2.0 * halfL));

    // Stand.
    painter->setPen(QPen(QColor(80, 90, 100), 2.0));
    painter->drawLine(QPointF(6.0, halfL), QPointF(6.0, halfL + 14.0));
    painter->drawLine(QPointF(0.0, halfL + 14.0), QPointF(12.0, halfL + 14.0));

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-10.0, halfL + 18.0, 48.0, 14.0),
                      Qt::AlignLeft, QString("%1nm").arg(cwl, 0, 'f', 0));
}

// ===========================================================================
// SlitComponent
// ===========================================================================

SlitComponent::SlitComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "OPT_SLIT";
    displayName = "Slit";
}

QRectF SlitComponent::boundingRect() const
{
    const double halfL = properties.value("screenLength", 120.0).toDouble() * 0.5 + 6.0;
    return QRectF(-18.0, -halfL, 52.0, 2.0 * halfL + 20.0);
}

void SlitComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfL    = properties.value("screenLength",   120.0).toDouble() * 0.5;
    const double slitW    = properties.value("slitWidth",       12.0).toDouble();
    const double slitSep  = properties.value("slitSeparation",  40.0).toDouble();
    const int    numSlits = std::clamp(properties.value("numSlits", 1).toInt(), 1, 2);
    const bool   selected = option->state & QStyle::State_Selected;

    const QPen screenPen = opticsPen(selected);
    painter->setPen(screenPen);
    painter->setBrush(QColor(50, 55, 60));

    // Draw opaque screen with gap(s) left open.
    // Determine gap regions along Y axis.
    QList<double> gapCentres;
    if (numSlits == 1) {
        gapCentres << 0.0;
    } else {
        gapCentres << -slitSep * 0.5 << slitSep * 0.5;
    }

    // Build list of opaque segments (sorted by Y).
    QList<QPair<double,double>> opaque;
    double prev = -halfL;
    for (double gc : gapCentres) {
        double gTop = gc - slitW * 0.5;
        double gBot = gc + slitW * 0.5;
        if (prev < gTop)
            opaque.append({prev, gTop});
        prev = gBot;
    }
    if (prev < halfL) opaque.append({prev, halfL});

    for (const auto& seg : opaque) {
        const double segH = seg.second - seg.first;
        painter->drawRect(QRectF(0.0, seg.first, 12.0, segH));
    }

    // Stand.
    painter->setPen(QPen(QColor(80, 90, 100), 2.0));
    painter->drawLine(QPointF(6.0, halfL), QPointF(6.0, halfL + 14.0));
    painter->drawLine(QPointF(0.0, halfL + 14.0), QPointF(12.0, halfL + 14.0));

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-10.0, halfL + 18.0, 48.0, 14.0),
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

    registry.registerType(
        opticsDescriptor("OPT_PRISM", "Prism",
                         "Glass prism with Snell refraction + Cauchy dispersion (n=A+B/λ²)",
                         {
                             { "sideLength",  100.0  },
                             { "apexAngle",    60.0  },   // deg (equilateral default)
                             { "cauchy_A",      1.458 },   // glass: n≈1.5 at 550nm
                             { "cauchy_B",  3.54e6   },   // nm²
                         }),
        [] { return new PrismComponent(); });

    registry.registerType(
        opticsDescriptor("OPT_FILTER", "Filter",
                         "Bandpass colour filter — only passes wavelengths within the window",
                         {
                             { "centerWavelength", 550.0 },   // nm
                             { "bandwidth",         80.0 },   // nm
                             { "transmittance",      0.90 },
                             { "length",            80.0  },   // px
                         }),
        [] { return new FilterComponent(); });

    registry.registerType(
        opticsDescriptor("OPT_SLIT", "Slit",
                         "Single or double slit aperture for diffraction experiments",
                         {
                             { "numSlits",        1    },
                             { "slitWidth",      12.0  },   // px
                             { "slitSeparation", 40.0  },   // px (double-slit)
                             { "screenLength",  120.0  },   // px
                         }),
        [] { return new SlitComponent(); });
}
