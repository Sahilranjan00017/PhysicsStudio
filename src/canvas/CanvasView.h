#pragma once

#include <QGraphicsView>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QJsonArray>
#include <QMap>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMouseEvent;
class BaseComponent;
class QGraphicsScene;
class QPainter;
class UndoRedoStack;

class CanvasView final : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget* parent = nullptr);
    QGraphicsScene* graphicsScene() const;
    QList<BaseComponent*> components() const;
    QJsonArray componentsToJson() const;
    void clearComponents();
    void deleteSelectedComponents();
    void loadComponents(const QJsonArray& components);
    void setUndoRedoStack(UndoRedoStack* stack);

signals:
    void componentPlaced(QString typeId, QPointF position);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QPointF snappedScenePosition(const QPoint& viewportPosition) const;

    QGraphicsScene* scene = nullptr;
    UndoRedoStack* undoRedoStack = nullptr;
    QMap<BaseComponent*, QPointF> dragStartPositions;
    int gridSize = 20;
};
