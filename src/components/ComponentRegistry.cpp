#include "components/ComponentRegistry.h"

#include "components/BaseComponent.h"
#include "components/electronics/ElectronicsComponents.h"

#include <utility>

ComponentRegistry& ComponentRegistry::instance()
{
    static ComponentRegistry registry;
    return registry;
}

void ComponentRegistry::registerType(const ComponentDescriptor& descriptor, Factory factory)
{
    if (descriptor.typeId.isEmpty()) {
        return;
    }

    componentDescriptors.insert(descriptor.typeId, descriptor);
    factories.insert(descriptor.typeId, std::move(factory));
}

BaseComponent* ComponentRegistry::create(const QString& typeId) const
{
    const auto factory = factories.constFind(typeId);
    if (factory == factories.constEnd()) {
        return nullptr;
    }

    BaseComponent* component = factory.value()();
    const auto descriptor = componentDescriptors.constFind(typeId);
    if (component != nullptr && descriptor != componentDescriptors.constEnd()) {
        component->typeId = descriptor->typeId;
        component->displayName = descriptor->displayName;
        component->properties = descriptor->defaultProperties;
    }
    return component;
}

QList<ComponentDescriptor> ComponentRegistry::descriptors() const
{
    return componentDescriptors.values();
}

bool ComponentRegistry::contains(const QString& typeId) const
{
    return factories.contains(typeId);
}

void registerCoreComponents(ComponentRegistry& registry)
{
    if (registry.contains("ELEC_RES")) {
        return;
    }

    registerElectronicsComponents(registry);
}
