#pragma once

#include <QMetaObject>
#include <QString>
#include <QVariant>
#include <QWidget>

class BaseComponent;
class QFormLayout;
class QLabel;
class UndoRedoStack;

class PropertiesPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    void setUndoRedoStack(UndoRedoStack* stack);

public slots:
    void setComponent(BaseComponent* component);

private:
    QVariant parsedValue(const QVariant& originalValue, const QString& text) const;
    void rebuildForm();
    void clearForm();

    BaseComponent* selectedComponent = nullptr;
    UndoRedoStack* undoRedoStack = nullptr;
    QMetaObject::Connection propertyChangedConnection;
    QLabel* titleLabel = nullptr;
    QFormLayout* formLayout = nullptr;
};
