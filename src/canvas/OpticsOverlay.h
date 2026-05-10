#pragma once

#include "simulation/optics/OpticalSolver.h"

#include <QGraphicsItem>
#include <QRectF>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// OpticsOverlay
// A transparent QGraphicsItem that renders optical ray paths on top of all
// other scene content.  It holds a pointer to the OpticalDomain::segments
// list owned by SimulationLoop; the segments are refreshed each solver tick.
//
// The item lives at scene position (0,0) with no transform, so local
// coordinates equal scene coordinates — ray segment endpoints (which are
// already in scene space) can be drawn directly.
// ---------------------------------------------------------------------------
class OpticsOverlay final : public QGraphicsItem {
public:
    // segments: reference to SimulationLoop's internal segment list.
    // Must remain valid for the overlay's lifetime.
    explicit OpticsOverlay(const QList<OpticalSegment>& segments,
                           QGraphicsItem* parent = nullptr);

    // QGraphicsItem interface.
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

private:
    const QList<OpticalSegment>& m_segments;  // non-owning reference
};
