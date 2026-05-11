#include "canvas/CanvasView.h"

#include "components/BaseComponent.h"
#include "components/ComponentMimeTypes.h"
#include "components/ComponentRegistry.h"
#include "components/Wire.h"
#include "interaction/AddPartCommand.h"
#include "interaction/AddWireCommand.h"
#include "interaction/DeletePartCommand.h"
#include "interaction/DeleteWireCommand.h"
#include "interaction/MovePartCommand.h"
#include "interaction/UndoRedoStack.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLineF>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QtGlobal>

namespace {
WireType wireTypeFromString(const QString& value)
{
    if (value == "power") {
        return WireType::Power;
    }
    if (value == "ground") {
        return WireType::Ground;
    }
    if (value == "optical") {
        return WireType::Optical;
    }
    if (value == "mechanical") {
        return WireType::Mechanical;
    }
    return WireType::Signal;
}
} // namespace

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
    setFocusPolicy(Qt::StrongFocus);  // ensure key events are received
}

QGraphicsScene* CanvasView::graphicsScene() const
{
    return scene;
}

QList<BaseComponent*> CanvasView::components() const
{
    QList<BaseComponent*> result;
    for (QGraphicsItem* item : scene->items()) {
        if (auto* component = dynamic_cast<BaseComponent*>(item)) {
            result.append(component);
        }
    }
    return result;
}

QList<Wire*> CanvasView::wires() const
{
    QList<Wire*> result;
    for (QGraphicsItem* item : scene->items()) {
        if (auto* wire = dynamic_cast<Wire*>(item)) {
            result.append(wire);
        }
    }
    return result;
}

QJsonArray CanvasView::componentsToJson() const
{
    QJsonArray array;
    for (BaseComponent* component : components()) {
        array.append(component->toJson());
    }
    return array;
}

QJsonArray CanvasView::wiresToJson() const
{
    QJsonArray array;
    for (Wire* wire : wires()) {
        array.append(wire->toJson());
    }
    return array;
}

void CanvasView::clearComponents()
{
    cancelWirePreview();
    scene->clear();
    dragStartPositions.clear();
}

void CanvasView::deleteSelectedComponents()
{
    const QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    QList<BaseComponent*> selectedComponents;
    for (QGraphicsItem* item : selectedItems) {
        if (auto* component = dynamic_cast<BaseComponent*>(item)) {
            selectedComponents.append(component);
        }
    }

    for (QGraphicsItem* item : selectedItems) {
        if (auto* wire = dynamic_cast<Wire*>(item)) {
            if (selectedComponents.contains(wire->startComponent) || selectedComponents.contains(wire->endComponent)) {
                continue;
            }

            if (undoRedoStack != nullptr) {
                undoRedoStack->push(new DeleteWireCommand(scene, wire));
            } else {
                scene->removeItem(wire);
                wire->detachFromPads();
                delete wire;
            }
            continue;
        }

        auto* component = dynamic_cast<BaseComponent*>(item);
        if (component == nullptr) {
            continue;
        }

        if (undoRedoStack != nullptr) {
            undoRedoStack->push(new DeletePartCommand(scene, component));
        } else {
            DeletePartCommand command(scene, component);
            command.redo();
        }
    }
}

void CanvasView::loadComponents(const QJsonArray& componentArray)
{
    loadScene(componentArray, {});
}

void CanvasView::loadScene(const QJsonArray& componentArray, const QJsonArray& wireArray)
{
    clearComponents();

    for (const QJsonValue& value : componentArray) {
        const QJsonObject object = value.toObject();
        const QString typeId = object["typeId"].toString();
        BaseComponent* component = ComponentRegistry::instance().create(typeId);
        if (component == nullptr) {
            continue;
        }

        component->fromJson(object);
        scene->addItem(component);
    }

    const QHash<QString, BaseComponent*> componentsById = componentIndex();
    for (const QJsonValue& value : wireArray) {
        const QJsonObject object = value.toObject();
        BaseComponent* startComponent = componentsById.value(object["startComponentId"].toString(), nullptr);
        BaseComponent* endComponent = componentsById.value(object["endComponentId"].toString(), nullptr);
        if (startComponent == nullptr || endComponent == nullptr) {
            continue;
        }

        ConnectionPad* startPad = startComponent->padById(object["startPadId"].toString());
        ConnectionPad* endPad = endComponent->padById(object["endPadId"].toString());
        if (startPad == nullptr || endPad == nullptr) {
            continue;
        }

        auto* wire = new Wire();
        wire->wireType = wireTypeFromString(object["wireType"].toString());
        wire->setEndpoints(startComponent, startPad, endComponent, endPad);
        scene->addItem(wire);
    }
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

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    // R — rotate each selected component 15° clockwise.
    if (event->key() == Qt::Key_R && !event->isAutoRepeat()) {
        const QList<QGraphicsItem*> sel = scene->selectedItems();
        for (QGraphicsItem* item : sel) {
            if (auto* comp = dynamic_cast<BaseComponent*>(item)) {
                comp->setComponentRotation(comp->rotationDegrees + 15.0);
            }
        }
        if (!sel.isEmpty()) {
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const PadHit hit = padAtScenePosition(mapToScene(event->pos()));
        if (hit.component != nullptr && hit.pad != nullptr) {
            wireStart = hit;
            previewWire = new Wire();
            previewWire->setPoints({
                hit.component->mapToScene(hit.pad->localPos),
                mapToScene(event->pos()),
            });
            previewWire->setZValue(-1.0);
            scene->addItem(previewWire);
            event->accept();
            return;
        }
    }

    dragStartPositions.clear();
    for (BaseComponent* component : components()) {
        dragStartPositions.insert(component, component->pos());
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (previewWire != nullptr && wireStart.component != nullptr && wireStart.pad != nullptr) {
        previewWire->setPoints({
            wireStart.component->mapToScene(wireStart.pad->localPos),
            mapToScene(event->pos()),
        });
        previewWire->update();
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (previewWire != nullptr) {
        const PadHit end = padAtScenePosition(mapToScene(event->pos()));
        scene->removeItem(previewWire);
        delete previewWire;
        previewWire = nullptr;

        if (padsCanConnect(wireStart, end)) {
            if (undoRedoStack != nullptr) {
                undoRedoStack->push(new AddWireCommand(scene, wireStart.component, wireStart.pad, end.component, end.pad));
            } else {
                AddWireCommand command(scene, wireStart.component, wireStart.pad, end.component, end.pad);
                command.redo();
            }
        }

        wireStart = {};
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);

    if (undoRedoStack == nullptr) {
        dragStartPositions.clear();
        return;
    }

    for (auto it = dragStartPositions.cbegin(); it != dragStartPositions.cend(); ++it) {
        BaseComponent* component = it.key();
        if (component == nullptr || component->scene() != scene) {
            continue;
        }

        const QPointF oldPosition = it.value();
        const QPointF newPosition = component->pos();
        if (oldPosition != newPosition) {
            undoRedoStack->push(new MovePartCommand(component, oldPosition, newPosition));
        }
    }

    dragStartPositions.clear();
}

QPointF CanvasView::snappedScenePosition(const QPoint& viewportPosition) const
{
    const QPointF scenePosition = mapToScene(viewportPosition);
    const double snappedX = qRound(scenePosition.x() / gridSize) * gridSize;
    const double snappedY = qRound(scenePosition.y() / gridSize) * gridSize;
    return QPointF(snappedX, snappedY);
}

CanvasView::PadHit CanvasView::padAtScenePosition(const QPointF& scenePosition) const
{
    PadHit result;
    double bestDistance = padHitRadius;

    for (BaseComponent* component : components()) {
        for (ConnectionPad* pad : component->pads) {
            if (pad == nullptr) {
                continue;
            }

            const QPointF padScenePosition = component->mapToScene(pad->localPos);
            const double distance = QLineF(scenePosition, padScenePosition).length();
            if (distance <= bestDistance) {
                result = { component, pad };
                bestDistance = distance;
            }
        }
    }

    return result;
}

bool CanvasView::padsCanConnect(const PadHit& start, const PadHit& end) const
{
    if (start.component == nullptr || start.pad == nullptr || end.component == nullptr || end.pad == nullptr) {
        return false;
    }
    if (start.component == end.component && start.pad == end.pad) {
        return false;
    }
    for (Wire* wire : start.pad->connectedWires) {
        if (wire == nullptr) {
            continue;
        }
        const bool sameDirection = wire->startPad == start.pad && wire->endPad == end.pad;
        const bool reverseDirection = wire->startPad == end.pad && wire->endPad == start.pad;
        if (sameDirection || reverseDirection) {
            return false;
        }
    }
    return start.pad->domain == end.pad->domain;
}

void CanvasView::cancelWirePreview()
{
    if (previewWire == nullptr) {
        wireStart = {};
        return;
    }

    scene->removeItem(previewWire);
    delete previewWire;
    previewWire = nullptr;
    wireStart = {};
}

QHash<QString, BaseComponent*> CanvasView::componentIndex() const
{
    QHash<QString, BaseComponent*> index;
    for (BaseComponent* component : components()) {
        if (component != nullptr && !component->id.isEmpty()) {
            index.insert(component->id, component);
        }
    }
    return index;
}
