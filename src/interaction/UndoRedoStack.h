#pragma once

#include <QObject>
#include <QUndoStack>

class QUndoCommand;

class UndoRedoStack final : public QObject {
    Q_OBJECT

public:
    explicit UndoRedoStack(QObject* parent = nullptr);

    void push(QUndoCommand* command);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    QUndoStack* qtStack();

private:
    QUndoStack stack;
};
