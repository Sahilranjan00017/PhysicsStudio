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
    constexpr float kMaxAmp = 2.5f;    // normalisation reference
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

WaveFieldOverlay::WaveFieldOverlay(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable,    false);
    setAcceptHoverEvents(false);
    setAcceptedMouseButtons(Qt::NoButton);
}

void WaveFieldOverlay::setField(std::vector<float> field, int cols, int rows, double gridSize)
{
    m_fieldBuf = std::move(field);
    m_cols     = cols;
    m_rows     = rows;
    m_gridSize = gridSize;
    update();   // schedule repaint on the main thread
}

QRectF WaveFieldOverlay::boundingRect() const
{
    // Fixed to match the default 120×80 grid at 20 px/cell = 2400×1600 scene units.
    return QRectF(0.0, 0.0, 2400.0, 1600.0);
}

void WaveFieldOverlay::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const int cols = m_cols;
    const int rows = m_rows;

    if (m_fieldBuf.empty() || cols <= 0 || rows <= 0)
        return;

    // Resize backing image when grid dimensions change.
    if (m_image.width() != cols || m_image.height() != rows)
        m_image = QImage(cols, rows, QImage::Format_ARGB32_Premultiplied);

    // Fill pixels directly from the owned amplitude buffer.
    const float* src = m_fieldBuf.data();
    for (int iy = 0; iy < rows; ++iy) {
        QRgb* line = reinterpret_cast<QRgb*>(m_image.scanLine(iy));
        for (int ix = 0; ix < cols; ++ix) {
            line[ix] = amplitudeToARGB(src[iy * cols + ix]);
        }
    }

    // Draw the low-resolution image scaled to cover the scene area, using
    // SmoothTransformation for a soft fluid appearance.
    const QRectF target(0.0, 0.0,
                        cols * m_gridSize,
                        rows * m_gridSize);

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(target, m_image, QRectF(0, 0, cols, rows));
}
