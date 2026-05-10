#pragma once

#include <QUndoCommand>

class QGraphicsScene;
class Wire;

class DeleteWireCommand final : public QUndoCommand {
public:
    DeleteWireCommand(QGraphicsScene* scene, Wire* wire, QUndoCommand* parent = nullptr);
    ~DeleteWireCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene* scene = nullptr;
    Wire* wire = nullptr;
    bool ownsWire = false;
};
