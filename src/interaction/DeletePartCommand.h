#pragma once

#include <QUndoCommand>
#include <QList>

class BaseComponent;
class QGraphicsScene;
class Wire;

class DeletePartCommand final : public QUndoCommand {
public:
    DeletePartCommand(QGraphicsScene* scene, BaseComponent* component, QUndoCommand* parent = nullptr);
    ~DeletePartCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene* scene = nullptr;
    BaseComponent* component = nullptr;
    QList<Wire*> attachedWires;
    bool ownsComponent = false;
};
