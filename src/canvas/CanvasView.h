#pragma once

#include <QGraphicsView>

class QGraphicsScene;

class CanvasView final : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget* parent = nullptr);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene* scene = nullptr;
    int gridSize = 20;
};

