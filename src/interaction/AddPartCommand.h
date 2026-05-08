#pragma once

#include <QPointF>
#include <QString>
#include <QUndoCommand>

class BaseComponent;
class QGraphicsScene;

class AddPartCommand final : public QUndoCommand {
public:
    AddPartCommand(QGraphicsScene* scene, QString typeId, QPointF position, QUndoCommand* parent = nullptr);
    ~AddPartCommand() override;

    void undo() override;
    void redo() override;

private:
    QGraphicsScene* scene = nullptr;
    QString typeId;
    QPointF position;
    BaseComponent* component = nullptr;
    bool ownsComponent = false;
};
