#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QMap>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QKeyEvent;
class QMouseEvent;
class BaseComponent;
class QGraphicsScene;
class QPainter;
class UndoRedoStack;
class Wire;
struct ConnectionPad;

class CanvasView final : public QGraphicsView {
    Q_OBJECT

public:
    explicit CanvasView(QWidget* parent = nullptr);
    QGraphicsScene* graphicsScene() const;
    QList<BaseComponent*> components() const;
    QList<Wire*> wires() const;
    QJsonArray componentsToJson() const;
    QJsonArray wiresToJson() const;
    void clearComponents();
    void deleteSelectedComponents();
    void loadComponents(const QJsonArray& components);
    void loadScene(const QJsonArray& components, const QJsonArray& wires);
    void setUndoRedoStack(UndoRedoStack* stack);

signals:
    void componentPlaced(QString typeId, QPointF position);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    struct PadHit {
        BaseComponent* component = nullptr;
        ConnectionPad* pad = nullptr;
    };

    QPointF snappedScenePosition(const QPoint& viewportPosition) const;
    PadHit padAtScenePosition(const QPointF& scenePosition) const;
    bool padsCanConnect(const PadHit& start, const PadHit& end) const;
    void cancelWirePreview();
    QHash<QString, BaseComponent*> componentIndex() const;

    QGraphicsScene* scene = nullptr;
    UndoRedoStack* undoRedoStack = nullptr;
    QMap<BaseComponent*, QPointF> dragStartPositions;
    PadHit wireStart;
    Wire* previewWire = nullptr;
    int gridSize = 20;
    double padHitRadius = 12.0;
};
