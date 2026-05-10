#pragma once

#include <QString>
#include <QUndoCommand>
#include <QVariant>

class BaseComponent;

class ChangePropertyCommand final : public QUndoCommand {
public:
    ChangePropertyCommand(
        BaseComponent* component,
        QString key,
        QVariant oldValue,
        QVariant newValue,
        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    BaseComponent* component = nullptr;
    QString key;
    QVariant oldValue;
    QVariant newValue;
};
