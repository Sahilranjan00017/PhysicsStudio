#pragma once

#include <QUndoCommand>

class BaseComponent;
class QGraphicsScene;
class Wire;
struct ConnectionPad;

class AddWireCommand final : public QUndoCommand {
public:
    AddWireCommand(
        QGraphicsScene* scene,
        BaseComponent* startComponent,
        ConnectionPad* startPad,
        BaseComponent* endComponent,
        ConnectionPad* endPad,
        QUndoCommand* parent = nullptr);
    ~AddWireCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene* scene = nullptr;
    BaseComponent* startComponent = nullptr;
    ConnectionPad* startPad = nullptr;
    BaseComponent* endComponent = nullptr;
    ConnectionPad* endPad = nullptr;
    Wire* wire = nullptr;
    bool ownsWire = false;
};
