#pragma once

#include "simulation/wave/WaveSolver.h"

#include <QGraphicsItem>
#include <QImage>
#include <QRectF>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// WaveFieldOverlay
// Renders the WaveDomain amplitude field as a colour-mapped image on a
// transparent QGraphicsItem sitting above all scene content (z = 40).
//
// Colour scheme:
//   positive amplitude → warm orange/red  (crest)
//   near-zero          → fully transparent (no tint)
//   negative amplitude → cool blue        (trough)
//
// The image is created at grid resolution (cols × rows) and drawn scaled to
// (cols × gridSize, rows × gridSize) scene pixels via smooth interpolation,
// giving a fluid-looking ripple pattern without per-pixel overdraw.
// ---------------------------------------------------------------------------
class WaveFieldOverlay final : public QGraphicsItem {
public:
    explicit WaveFieldOverlay(const WaveDomain& domain,
                              QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

private:
    const WaveDomain& m_domain;
    mutable QImage    m_image;   // reused each frame; resized when grid dims change
};
