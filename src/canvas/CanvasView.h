#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QString>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QGraphicsScene;

class CanvasView final : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget* parent = nullptr);

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
    int gridSize = 20;
};
