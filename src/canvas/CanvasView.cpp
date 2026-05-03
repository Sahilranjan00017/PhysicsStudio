#include "canvas/CanvasView.h"

#include <QGraphicsScene>
#include <QPainter>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent),
      scene(new QGraphicsScene(this))
{
    scene->setSceneRect(0, 0, 2400, 1600);
    setScene(scene);
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void CanvasView::drawBackground(QPainter* painter, const QRectF& rect)
{
    QGraphicsView::drawBackground(painter, rect);

    const int left = static_cast<int>(rect.left()) - static_cast<int>(rect.left()) % gridSize;
    const int top = static_cast<int>(rect.top()) - static_cast<int>(rect.top()) % gridSize;

    QVector<QLineF> lines;
    for (int x = left; x < rect.right(); x += gridSize) {
        lines.append(QLineF(x, rect.top(), x, rect.bottom()));
    }
    for (int y = top; y < rect.bottom(); y += gridSize) {
        lines.append(QLineF(rect.left(), y, rect.right(), y));
    }

    painter->setPen(QPen(QColor(230, 234, 238), 1));
    painter->drawLines(lines);
}

