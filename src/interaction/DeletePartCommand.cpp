#include "interaction/DeletePartCommand.h"

#include "components/BaseComponent.h"

#include <QGraphicsScene>
#include <QString>

DeletePartCommand::DeletePartCommand(QGraphicsScene* scene, BaseComponent* component, QUndoCommand* parent)
    : QUndoCommand(parent),
      scene(scene),
      component(component)
{
    setText(component == nullptr ? "Delete part" : QString("Delete %1").arg(component->displayName));
}

DeletePartCommand::~DeletePartCommand()
{
    if (ownsComponent) {
        delete component;
    }
}

void DeletePartCommand::undo()
{
    if (scene == nullptr || component == nullptr) {
        return;
    }

    scene->addItem(component);
    ownsComponent = false;
}

void DeletePartCommand::redo()
{
    if (scene == nullptr || component == nullptr) {
        return;
    }

    scene->removeItem(component);
    ownsComponent = true;
}
