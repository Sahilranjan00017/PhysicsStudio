#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QGraphicsScene;
class QPainter;
class UndoRedoStack;

class CanvasView final : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget* parent = nullptr);
    QGraphicsScene* graphicsScene() const;
    void setUndoRedoStack(UndoRedoStack* stack);

signals:
    void componentPlaced(QString typeId, QPointF position);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    QPointF snappedScenePosition(const QPoint& viewportPosition) const;

    QGraphicsScene* scene = nullptr;
    UndoRedoStack* undoRedoStack = nullptr;
    int gridSize = 20;
};
