#include "interaction/DeletePartCommand.h"

#include "components/BaseComponent.h"
#include "components/Wire.h"

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
        for (Wire* wire : attachedWires) {
            delete wire;
        }
        delete component;
    }
}

void DeletePartCommand::undo()
{
    if (scene == nullptr || component == nullptr) {
        return;
    }

    scene->addItem(component);
    for (Wire* wire : attachedWires) {
        wire->attachToPads();
        wire->reroute();
        scene->addItem(wire);
    }
    ownsComponent = false;
}

void DeletePartCommand::redo()
{
    if (scene == nullptr || component == nullptr) {
        return;
    }

    if (attachedWires.isEmpty()) {
        for (ConnectionPad* pad : component->pads) {
            if (pad == nullptr) {
                continue;
            }
            for (Wire* wire : pad->connectedWires) {
                if (wire != nullptr && !attachedWires.contains(wire)) {
                    attachedWires.append(wire);
                }
            }
        }
    }

    for (Wire* wire : attachedWires) {
        scene->removeItem(wire);
        wire->detachFromPads();
    }
    scene->removeItem(component);
    ownsComponent = true;
}
