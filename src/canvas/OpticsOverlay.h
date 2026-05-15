#pragma once

#include "simulation/optics/OpticalSolver.h"

#include <QGraphicsItem>
#include <QList>
#include <QRectF>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

// ---------------------------------------------------------------------------
// OpticsOverlay
// A transparent QGraphicsItem that renders optical ray paths on top of all
// other scene content (z = 50).
//
// Thread safety: the overlay owns its own segment buffer (m_segsBuf).
// The simulation worker thread sends fresh segments via
// SimulationLoop::opticsUpdated, which is connected to setSegments() via
// Qt::QueuedConnection so the copy arrives on the main thread before paint()
// can read it.  No locking is required.
// ---------------------------------------------------------------------------
class OpticsOverlay final : public QGraphicsItem {
public:
    explicit OpticsOverlay(QGraphicsItem* parent = nullptr);

    // Called on the main thread (via Qt::QueuedConnection) with the latest
    // ray segments emitted by SimulationLoop after each optics-solver tick.
    void setSegments(QList<OpticalSegment> segments);

    // QGraphicsItem interface.
    QRectF boundingRect() const override;
    void   paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

private:
    QList<OpticalSegment> m_segsBuf;    // owned copy of the latest ray list
};
