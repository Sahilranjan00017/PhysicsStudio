#pragma once

#include <QPointF>
#include <QUndoCommand>

class BaseComponent;

class MovePartCommand final : public QUndoCommand {
public:
    MovePartCommand(BaseComponent* component, QPointF oldPosition, QPointF newPosition, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    BaseComponent* component = nullptr;
    QPointF oldPosition;
    QPointF newPosition;
};
