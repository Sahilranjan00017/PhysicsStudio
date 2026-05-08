#include "canvas/CanvasView.h"

#include "components/ComponentMimeTypes.h"
#include "components/ComponentRegistry.h"
#include "interaction/AddPartCommand.h"
#include "interaction/UndoRedoStack.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QMimeData>
#include <QPainter>
#include <QtGlobal>

CanvasView::CanvasView(QWidget* parent)
    : QGraphicsView(parent),
      scene(new QGraphicsScene(this))
{
    scene->setSceneRect(0, 0, 2400, 1600);
    setScene(scene);
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setAcceptDrops(true);
}

QGraphicsScene* CanvasView::graphicsScene() const
{
    return scene;
}

void CanvasView::setUndoRedoStack(UndoRedoStack* stack)
{
    undoRedoStack = stack;
}

void CanvasView::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(ComponentMimeTypes::componentType)) {
        event->acceptProposedAction();
        return;
    }

    QGraphicsView::dragEnterEvent(event);
}

void CanvasView::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(ComponentMimeTypes::componentType)) {
        event->acceptProposedAction();
        return;
    }

    QGraphicsView::dragMoveEvent(event);
}

void CanvasView::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(ComponentMimeTypes::componentType)) {
        QGraphicsView::dropEvent(event);
        return;
    }

    const QPoint viewportPosition = event->position().toPoint();
    const QPointF scenePosition = snappedScenePosition(viewportPosition);
    const QString typeId = QString::fromUtf8(event->mimeData()->data(ComponentMimeTypes::componentType));
    if (!ComponentRegistry::instance().contains(typeId)) {
        event->ignore();
        return;
    }

    if (undoRedoStack != nullptr) {
        undoRedoStack->push(new AddPartCommand(scene, typeId, scenePosition));
    } else {
        AddPartCommand command(scene, typeId, scenePosition);
        command.redo();
    }

    emit componentPlaced(typeId, scenePosition);
    event->acceptProposedAction();
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

QPointF CanvasView::snappedScenePosition(const QPoint& viewportPosition) const
{
    const QPointF scenePosition = mapToScene(viewportPosition);
    const double snappedX = qRound(scenePosition.x() / gridSize) * gridSize;
    const double snappedY = qRound(scenePosition.y() / gridSize) * gridSize;
    return QPointF(snappedX, snappedY);
}
