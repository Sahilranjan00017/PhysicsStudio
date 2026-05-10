#pragma once

#include <QUndoCommand>

class BaseComponent;
class QGraphicsScene;

class DeletePartCommand final : public QUndoCommand {
public:
    DeletePartCommand(QGraphicsScene* scene, BaseComponent* component, QUndoCommand* parent = nullptr);
    ~DeletePartCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene* scene = nullptr;
    BaseComponent* component = nullptr;
    bool ownsComponent = false;
};
