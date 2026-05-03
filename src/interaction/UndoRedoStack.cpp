#include "interaction/UndoRedoStack.h"

UndoRedoStack::UndoRedoStack(QObject* parent)
    : QObject(parent)
{
}

void UndoRedoStack::push(QUndoCommand* command)
{
    stack.push(command);
}

void UndoRedoStack::undo()
{
    stack.undo();
}

void UndoRedoStack::redo()
{
    stack.redo();
}

bool UndoRedoStack::canUndo() const
{
    return stack.canUndo();
}

bool UndoRedoStack::canRedo() const
{
    return stack.canRedo();
}

