#include "ui/properties/PropertiesPanel.h"

#include "components/BaseComponent.h"
#include "interaction/ChangePropertyCommand.h"
#include "interaction/UndoRedoStack.h"

#include <QFormLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMetaType>
#include <QVBoxLayout>
#include <QVariant>

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent),
      titleLabel(new QLabel("No component selected", this)),
      formLayout(new QFormLayout())
{
    titleLabel->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(titleLabel);
    layout->addLayout(formLayout);
    layout->addStretch();
}

void PropertiesPanel::setComponent(BaseComponent* component)
{
    if (propertyChangedConnection) {
        disconnect(propertyChangedConnection);
        propertyChangedConnection = {};
    }

    selectedComponent = component;
    if (selectedComponent != nullptr) {
        propertyChangedConnection = connect(
            selectedComponent,
            &BaseComponent::propertyChanged,
            this,
            [this](const QString&, const QVariant&) {
                rebuildForm();
            });
    }

    rebuildForm();
}

void PropertiesPanel::setUndoRedoStack(UndoRedoStack* stack)
{
    undoRedoStack = stack;
}

QVariant PropertiesPanel::parsedValue(const QVariant& originalValue, const QString& text) const
{
    bool ok = false;

    switch (originalValue.metaType().id()) {
    case QMetaType::Bool:
        return text.compare("true", Qt::CaseInsensitive) == 0 || text == "1";
    case QMetaType::Int: {
        const int value = text.toInt(&ok);
        return ok ? QVariant(value) : originalValue;
    }
    case QMetaType::Double: {
        const double value = text.toDouble(&ok);
        return ok ? QVariant(value) : originalValue;
    }
    default:
        return text;
    }
}

void PropertiesPanel::rebuildForm()
{
    clearForm();

    if (selectedComponent == nullptr) {
        titleLabel->setText("No component selected");
        return;
    }

    titleLabel->setText(QString("%1\n%2").arg(selectedComponent->displayName, selectedComponent->typeId));

    for (auto it = selectedComponent->properties.cbegin(); it != selectedComponent->properties.cend(); ++it) {
        const QString key = it.key();
        const QVariant originalValue = it.value();
        BaseComponent* component = selectedComponent;
        auto* editor = new QLineEdit(originalValue.toString(), this);
        editor->setEnabled(!selectedComponent->lockedProperties);

        connect(editor, &QLineEdit::editingFinished, this, [this, component, key, originalValue, editor]() {
            if (selectedComponent != component || component == nullptr) {
                return;
            }

            const QVariant newValue = parsedValue(originalValue, editor->text());
            const QVariant currentValue = component->properties.value(key);
            if (newValue == currentValue) {
                return;
            }

            if (undoRedoStack != nullptr) {
                undoRedoStack->push(new ChangePropertyCommand(component, key, currentValue, newValue));
            } else {
                component->setComponentProperty(key, newValue);
            }
        });

        formLayout->addRow(key, editor);
    }
}

void PropertiesPanel::clearForm()
{
    while (QLayoutItem* item = formLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
