#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

#include <functional>

class BaseComponent;

struct ComponentDescriptor {
    QString typeId;
    QString displayName;
    QString categoryPath;
    QString description;
    QMap<QString, QVariant> defaultProperties;
};

class ComponentRegistry final {
public:
    using Factory = std::function<BaseComponent*()>;

    static ComponentRegistry& instance();

    void registerType(const ComponentDescriptor& descriptor, Factory factory);
    BaseComponent* create(const QString& typeId) const;
    QList<ComponentDescriptor> descriptors() const;
    bool contains(const QString& typeId) const;

private:
    ComponentRegistry() = default;

    QMap<QString, ComponentDescriptor> componentDescriptors;
    QMap<QString, Factory> factories;
};

void registerCoreComponents(ComponentRegistry& registry);
