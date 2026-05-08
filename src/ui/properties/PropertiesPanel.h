#pragma once

#include <QString>
#include <QVariant>
#include <QWidget>

class BaseComponent;
class QFormLayout;
class QLabel;

class PropertiesPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

public slots:
    void setComponent(BaseComponent* component);

private:
    QVariant parsedValue(const QVariant& originalValue, const QString& text) const;
    void rebuildForm();
    void clearForm();

    BaseComponent* selectedComponent = nullptr;
    QLabel* titleLabel = nullptr;
    QFormLayout* formLayout = nullptr;
};
