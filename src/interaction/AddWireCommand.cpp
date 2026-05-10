#include "interaction/AddWireCommand.h"

#include "components/BaseComponent.h"
#include "components/ConnectionPad.h"
#include "components/Wire.h"

#include <QGraphicsScene>

AddWireCommand::AddWireCommand(
    QGraphicsScene* scene,
    BaseComponent* startComponent,
    ConnectionPad* startPad,
    BaseComponent* endComponent,
    ConnectionPad* endPad,
    QUndoCommand* parent)
    : QUndoCommand(parent),
      scene(scene),
      startComponent(startComponent),
      startPad(startPad),
      endComponent(endComponent),
      endPad(endPad)
{
    setText("Add wire");
}

AddWireCommand::~AddWireCommand()
{
    if (ownsWire) {
        delete wire;
    }
}

void AddWireCommand::undo()
{
    if (scene == nullptr || wire == nullptr) {
        return;
    }

    scene->removeItem(wire);
    wire->detachFromPads();
    ownsWire = true;
}

void AddWireCommand::redo()
{
    if (scene == nullptr) {
        return;
    }

    if (wire == nullptr) {
        wire = new Wire();
        // Infer wire type from the pad domain so mechanical pads get a green
        // spring-style wire and electrical pads get the default blue wire.
        if (startPad != nullptr) {
            switch (startPad->domain) {
            case DomainType::Mechanical:
                wire->wireType = WireType::Mechanical; break;
            case DomainType::Optical:
                wire->wireType = WireType::Optical; break;
            default:
                wire->wireType = WireType::Signal; break;
            }
        }
        wire->setEndpoints(startComponent, startPad, endComponent, endPad);
    } else {
        wire->attachToPads();
        wire->reroute();
    }

    scene->addItem(wire);
    ownsWire = false;
}
