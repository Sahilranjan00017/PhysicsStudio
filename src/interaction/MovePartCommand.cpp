#include "interaction/MovePartCommand.h"

#include "components/BaseComponent.h"

#include <QString>

MovePartCommand::MovePartCommand(
    BaseComponent* component,
    QPointF oldPosition,
    QPointF newPosition,
    QUndoCommand* parent)
    : QUndoCommand(parent),
      component(component),
      oldPosition(oldPosition),
      newPosition(newPosition)
{
    setText(component == nullptr ? "Move part" : QString("Move %1").arg(component->displayName));
}

void MovePartCommand::undo()
{
    if (component != nullptr) {
        component->setPos(oldPosition);
    }
}

void MovePartCommand::redo()
{
    if (component != nullptr) {
        component->setPos(newPosition);
    }
}
