#include "interaction/ChangePropertyCommand.h"

#include "components/BaseComponent.h"

#include <utility>

ChangePropertyCommand::ChangePropertyCommand(
    BaseComponent* component,
    QString key,
    QVariant oldValue,
    QVariant newValue,
    QUndoCommand* parent)
    : QUndoCommand(parent),
      component(component),
      key(std::move(key)),
      oldValue(std::move(oldValue)),
      newValue(std::move(newValue))
{
    setText(component == nullptr ? "Change property" : QString("Change %1").arg(this->key));
}

void ChangePropertyCommand::undo()
{
    if (component != nullptr) {
        component->setComponentProperty(key, oldValue);
    }
}

void ChangePropertyCommand::redo()
{
    if (component != nullptr) {
        component->setComponentProperty(key, newValue);
    }
}
