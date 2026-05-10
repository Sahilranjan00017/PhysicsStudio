#include "canvas/WaveFieldOverlay.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Colour mapping: amplitude → ARGB pixel
// ---------------------------------------------------------------------------
namespace {

inline QRgb amplitudeToARGB(float amp)
{
    constexpr float kMaxAmp = 2.5f;    // normalization reference
    const float n = std::clamp(amp / kMaxAmp, -1.0f, 1.0f);  // –1 … +1
    const float a = std::abs(n);

    if (a < 0.04f)
        return qRgba(0, 0, 0, 0);      // transparent for near-zero

    const int alpha = static_cast<int>(a * 190.0f);   // max opacity 190/255

    if (n > 0.0f) {
        // Crest: orange → red
        const int r = 255;
        const int g = static_cast<int>((1.0f - n * 0.85f) * 160.0f);
        const int b = 0;
        return qRgba(r, g, b, alpha);
    } else {
        // Trough: cyan-blue
        const int r = 0;
        const int g = static_cast<int>((1.0f + n * 0.5f) * 180.0f);
        const int b = 255;
        return qRgba(r, g, b, alpha);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// WaveFieldOverlay
// ---------------------------------------------------------------------------

WaveFieldOverlay::WaveFieldOverlay(const WaveDomain& domain, QGraphicsItem* parent)
    : QGraphicsItem(parent),
      m_domain(domain)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable,    false);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);
}

QRectF WaveFieldOverlay::boundingRect() const
{
    return QRectF(0.0, 0.0, 2400.0, 1600.0);
}

void WaveFieldOverlay::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const int cols = m_domain.cols;
    const int rows = m_domain.rows;

    if (m_domain.field.empty() || cols <= 0 || rows <= 0)
        return;

    // Resize backing image when grid dimensions change.
    if (m_image.width() != cols || m_image.height() != rows)
        m_image = QImage(cols, rows, QImage::Format_ARGB32_Premultiplied);

    // Fill pixels directly from the amplitude field.
    const float* src = m_domain.field.data();
    for (int iy = 0; iy < rows; ++iy) {
        QRgb* line = reinterpret_cast<QRgb*>(m_image.scanLine(iy));
        for (int ix = 0; ix < cols; ++ix) {
            line[ix] = amplitudeToARGB(src[iy * cols + ix]);
        }
    }

    // Draw the low-resolution image scaled to cover the scene area, using
    // SmoothTransformation for a soft fluid appearance.
    const QRectF target(0.0, 0.0,
                        cols * m_domain.gridSize,
                        rows * m_domain.gridSize);

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(target, m_image, QRectF(0, 0, cols, rows));
}
