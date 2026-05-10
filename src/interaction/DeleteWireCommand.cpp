#include "interaction/DeleteWireCommand.h"

#include "components/Wire.h"

#include <QGraphicsScene>

DeleteWireCommand::DeleteWireCommand(QGraphicsScene* scene, Wire* wire, QUndoCommand* parent)
    : QUndoCommand(parent),
      scene(scene),
      wire(wire)
{
    setText("Delete wire");
}

DeleteWireCommand::~DeleteWireCommand()
{
    if (ownsWire) {
        delete wire;
    }
}

void DeleteWireCommand::undo()
{
    if (scene == nullptr || wire == nullptr) {
        return;
    }

    wire->attachToPads();
    wire->reroute();
    scene->addItem(wire);
    ownsWire = false;
}

void DeleteWireCommand::redo()
{
    if (scene == nullptr || wire == nullptr) {
        return;
    }

    scene->removeItem(wire);
    wire->detachFromPads();
    ownsWire = true;
}
