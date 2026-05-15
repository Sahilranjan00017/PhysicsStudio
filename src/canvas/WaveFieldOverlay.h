#pragma once

#include <QGraphicsItem>
#include <QImage>
#include <QRectF>

#include <vector>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// WaveFieldOverlay
// Renders a wave amplitude field as a colour-mapped image on a transparent
// QGraphicsItem sitting above all scene content (z = 40).
//
// Thread safety: the overlay owns its own field buffer (m_fieldBuf).
// The simulation worker thread sends fresh data via SimulationLoop::waveFieldUpdated,
// which is connected to setField() via Qt::QueuedConnection so the copy lands on
// the main thread before paint() can read it.  No locking is required.
//
// Colour scheme:
//   positive amplitude → warm orange/red  (crest)
//   near-zero          → fully transparent (no tint)
//   negative amplitude → cool blue        (trough)
// ---------------------------------------------------------------------------
class WaveFieldOverlay final : public QGraphicsItem {
public:
    explicit WaveFieldOverlay(QGraphicsItem* parent = nullptr);

    // Called on the main thread (via Qt::QueuedConnection) with fresh field data
    // emitted by SimulationLoop after each wave-solver tick.
    void setField(std::vector<float> field, int cols, int rows, double gridSize);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

private:
    std::vector<float> m_fieldBuf;          // owned copy of the latest field
    int    m_cols     = 120;
    int    m_rows     = 80;
    double m_gridSize = 20.0;
    mutable QImage m_image;                 // reused across frames; resized when dims change
};
