#include "interaction/AddPartCommand.h"

#include "components/BaseComponent.h"
#include "components/ComponentRegistry.h"

#include <QGraphicsScene>
#include <QUuid>

#include <utility>

AddPartCommand::AddPartCommand(QGraphicsScene* scene, QString typeId, QPointF position, QUndoCommand* parent)
    : QUndoCommand(parent),
      scene(scene),
      typeId(std::move(typeId)),
      position(position)
{
    setText(QString("Add %1").arg(this->typeId));
}

AddPartCommand::~AddPartCommand()
{
    if (ownsComponent) {
        delete component;
    }
}

void AddPartCommand::undo()
{
    if (scene == nullptr || component == nullptr) {
        return;
    }

    scene->removeItem(component);
    ownsComponent = true;
}

void AddPartCommand::redo()
{
    if (scene == nullptr) {
        return;
    }

    if (component == nullptr) {
        component = ComponentRegistry::instance().create(typeId);
        if (component == nullptr) {
            return;
        }
        component->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        component->setPos(position);
    }

    scene->addItem(component);
    ownsComponent = false;
}
