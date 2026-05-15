#include "canvas/OpticsOverlay.h"

#include <QColor>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Wavelength → QColor (visible spectrum approximation)
// ---------------------------------------------------------------------------
namespace {

QColor wavelengthToColor(double nm, double intensity)
{
    double r, g, b;
    if      (nm < 380)              { r = 0.6; g = 0.0; b = 0.8; }
    else if (nm < 440) { double t = (nm - 380) / 60.0; r = 1.0 - t; g = 0.0;       b = 1.0; }
    else if (nm < 490) { double t = (nm - 440) / 50.0; r = 0.0;       g = t;        b = 1.0; }
    else if (nm < 510) { double t = (nm - 490) / 20.0; r = 0.0;       g = 1.0;      b = 1.0 - t; }
    else if (nm < 580) { double t = (nm - 510) / 70.0; r = t;         g = 1.0;      b = 0.0; }
    else if (nm < 645) { double t = (nm - 580) / 65.0; r = 1.0;       g = 1.0 - t;  b = 0.0; }
    else if (nm <= 700)             { r = 1.0; g = 0.0; b = 0.0; }
    else                            { r = 0.5; g = 0.0; b = 0.0; }

    return QColor::fromRgbF(
        std::clamp(r * intensity, 0.0, 1.0),
        std::clamp(g * intensity, 0.0, 1.0),
        std::clamp(b * intensity, 0.0, 1.0),
        std::clamp(0.82 * intensity, 0.0, 1.0));
}

} // namespace

// ---------------------------------------------------------------------------
// OpticsOverlay
// ---------------------------------------------------------------------------

OpticsOverlay::OpticsOverlay(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    // Non-interactive: rays should not be selectable or block mouse events.
    setFlag(QGraphicsItem::ItemIsSelectable,   false);
    setFlag(QGraphicsItem::ItemIsMovable,      false);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);
}

void OpticsOverlay::setSegments(QList<OpticalSegment> segments)
{
    m_segsBuf = std::move(segments);
    update();   // schedule repaint on the main thread
}

QRectF OpticsOverlay::boundingRect() const
{
    // Cover the full scene rect so rays are always rendered regardless of
    // where they travel.  Matches the scene rect set in CanvasView.
    return QRectF(0.0, 0.0, 2400.0, 1600.0);
}

void OpticsOverlay::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (m_segsBuf.isEmpty())
        return;

    painter->setRenderHint(QPainter::Antialiasing, true);

    for (const OpticalSegment& seg : m_segsBuf) {
        const QColor color = wavelengthToColor(seg.wavelength, seg.intensity);
        painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(seg.start, seg.end);
    }
}
