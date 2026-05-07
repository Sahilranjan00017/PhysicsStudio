# Component Registry Design: Factory Pattern

This document outlines the component registry and factory system for instantiating components. It covers registration, discovery, and property definition.

---

## Overview

The **Component Registry** is a central repository of available component types and how to create them. It uses the **Factory Pattern** to decouple component creation from the UI and solver layers.

**Key benefits**:
- Single point for component type management
- Type-safe component creation
- Property schemas defined per component
- Easy to add new components (just register and implement)
- UI (Parts Panel) can query registry for available types

---

## Architecture

### ComponentFactory (Static Registry)

```cpp
class ComponentFactory {
public:
    // Register a component type
    static void registerComponent(
        const QString& typeId,
        const ComponentInfo& info
    );
    
    // Create instance of registered type
    static BaseComponent* createComponent(const QString& typeId);
    
    // Query available types
    static QStringList registeredTypes();
    static ComponentInfo getInfo(const QString& typeId);
    static QList<ComponentInfo> allComponents();
    
    // Filter by domain
    static QList<ComponentInfo> componentsInDomain(DomainType domain);

private:
    static Map<QString, ComponentInfo> registry;
};
```

### ComponentInfo (Metadata)

```cpp
struct ComponentInfo {
    QString typeId;              // Unique identifier ("Resistor", "Ball")
    QString displayName;         // User-friendly name ("Resistor")
    QString category;            // "Electronics", "Motion", "Optics", "Wave"
    QString description;         // Tooltip/help text
    DomainType domain;           // Which physics domain
    
    QList<PropertyDefinition> properties;  // Editable properties
    
    std::function<BaseComponent*()> creator;  // Factory function
    
    QIcon icon;                  // UI icon for Parts Panel
    QPixmap preview;             // Small preview image
};
```

### PropertyDefinition (Property Schema)

```cpp
struct PropertyDefinition {
    QString name;               // Property key ("resistance", "mass")
    QString displayName;        // UI label ("Resistance")
    QString type;              // "double", "int", "string", "bool"
    QVariant defaultValue;     // Default if not specified
    QVariant minValue;         // Min for spinbox/slider (optional)
    QVariant maxValue;         // Max for spinbox/slider (optional)
    QString unit;              // "ohms", "kg", "meters" (for display)
    QString description;       // Tooltip explaining the property
    
    bool readOnly = false;     // If true, not editable
};
```

---

## Registration System

### Bootstrap (in Main)

```cpp
// main.cpp
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // Register all components
    ComponentRegistry::registerAll();
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
```

### Central Registration Function

```cpp
// components/ComponentRegistry.cpp

void ComponentRegistry::registerAll() {
    registerElectronicsComponents();
    registerMotionComponents();
    registerOpticsComponents();
    registerWaveComponents();
}

void ComponentRegistry::registerElectronicsComponents() {
    // Resistor
    ComponentFactory::registerComponent("Resistor", {
        .typeId = "Resistor",
        .displayName = "Resistor",
        .category = "Electronics",
        .description = "Passive two-terminal component with fixed resistance",
        .domain = DomainType::Electrical,
        .properties = {
            PropertyDefinition{
                .name = "resistance",
                .displayName = "Resistance",
                .type = "double",
                .defaultValue = 1000.0,
                .minValue = 0.1,
                .maxValue = 1e9,
                .unit = "Ω",
                .description = "Resistance value in ohms"
            },
            PropertyDefinition{
                .name = "label",
                .displayName = "Label",
                .type = "string",
                .defaultValue = "R",
                .description = "Component label (e.g., R1, R2)"
            }
        },
        .creator = []() { return new Resistor(); },
        .icon = QIcon(":/icons/resistor.png"),
        .preview = QPixmap(":/previews/resistor.png")
    });
    
    // Capacitor
    ComponentFactory::registerComponent("Capacitor", {
        .typeId = "Capacitor",
        .displayName = "Capacitor",
        .category = "Electronics",
        .description = "Passive two-terminal component storing electrical energy",
        .domain = DomainType::Electrical,
        .properties = {
            PropertyDefinition{
                .name = "capacitance",
                .displayName = "Capacitance",
                .type = "double",
                .defaultValue = 1e-6,  // 1 µF
                .minValue = 1e-12,
                .maxValue = 1.0,
                .unit = "F",
                .description = "Capacitance in farads"
            }
        },
        .creator = []() { return new Capacitor(); }
    });
    
    // VoltageSource
    ComponentFactory::registerComponent("VoltageSource", {
        .typeId = "VoltageSource",
        .displayName = "Voltage Source",
        .category = "Electronics",
        .description = "Voltage source (battery or DC supply)",
        .domain = DomainType::Electrical,
        .properties = {
            PropertyDefinition{
                .name = "voltage",
                .displayName = "Voltage",
                .type = "double",
                .defaultValue = 5.0,
                .minValue = -100.0,
                .maxValue = 100.0,
                .unit = "V",
                .description = "Voltage value in volts"
            }
        },
        .creator = []() { return new VoltageSource(); }
    });
    
    // CurrentSource
    ComponentFactory::registerComponent("CurrentSource", {
        .typeId = "CurrentSource",
        .displayName = "Current Source",
        .category = "Electronics",
        .description = "Current source (independent current supply)",
        .domain = DomainType::Electrical,
        .properties = {
            PropertyDefinition{
                .name = "current",
                .displayName = "Current",
                .type = "double",
                .defaultValue = 0.001,  // 1 mA
                .minValue = -10.0,
                .maxValue = 10.0,
                .unit = "A",
                .description = "Current value in amperes"
            }
        },
        .creator = []() { return new CurrentSource(); }
    });
    
    // Ammeter (ideal current meter)
    ComponentFactory::registerComponent("Ammeter", {
        .typeId = "Ammeter",
        .displayName = "Ammeter",
        .category = "Electronics",
        .description = "Ideal ammeter (zero resistance, measures current)",
        .domain = DomainType::Electrical,
        .properties = {
            PropertyDefinition{
                .name = "label",
                .displayName = "Label",
                .type = "string",
                .defaultValue = "A",
                .description = "Meter label"
            }
        },
        .creator = []() { return new Ammeter(); }
    });
    
    // Voltmeter (ideal voltage meter)
    ComponentFactory::registerComponent("Voltmeter", {
        .typeId = "Voltmeter",
        .displayName = "Voltmeter",
        .category = "Electronics",
        .description = "Ideal voltmeter (infinite resistance, measures voltage)",
        .domain = DomainType::Electrical,
        .properties = {
            PropertyDefinition{
                .name = "label",
                .displayName = "Label",
                .type = "string",
                .defaultValue = "V",
                .description = "Meter label"
            }
        },
        .creator = []() { return new Voltmeter(); }
    });
}

void ComponentRegistry::registerMotionComponents() {
    // Ball
    ComponentFactory::registerComponent("Ball", {
        .typeId = "Ball",
        .displayName = "Ball",
        .category = "Motion",
        .description = "Sphere with mass and gravity",
        .domain = DomainType::Mechanical,
        .properties = {
            PropertyDefinition{
                .name = "mass",
                .displayName = "Mass",
                .type = "double",
                .defaultValue = 1.0,
                .minValue = 0.001,
                .maxValue = 1000.0,
                .unit = "kg",
                .description = "Ball mass in kilograms"
            },
            PropertyDefinition{
                .name = "radius",
                .displayName = "Radius",
                .type = "double",
                .defaultValue = 0.1,
                .minValue = 0.01,
                .maxValue = 10.0,
                .unit = "m",
                .description = "Ball radius in meters"
            }
        },
        .creator = []() { return new Ball(); }
    });
    
    // Block
    ComponentFactory::registerComponent("Block", {
        .typeId = "Block",
        .displayName = "Block",
        .category = "Motion",
        .description = "Rectangular rigid body",
        .domain = DomainType::Mechanical,
        .properties = {
            PropertyDefinition{
                .name = "mass",
                .displayName = "Mass",
                .type = "double",
                .defaultValue = 1.0,
                .unit = "kg"
            },
            PropertyDefinition{
                .name = "width",
                .displayName = "Width",
                .type = "double",
                .defaultValue = 0.2,
                .unit = "m"
            },
            PropertyDefinition{
                .name = "height",
                .displayName = "Height",
                .type = "double",
                .defaultValue = 0.2,
                .unit = "m"
            }
        },
        .creator = []() { return new Block(); }
    });
    
    // Spring (stub for now)
    // Hinge (stub for now)
}

void ComponentRegistry::registerOpticsComponents() {
    // Mirror (stub)
    // Lens (stub)
}

void ComponentRegistry::registerWaveComponents() {
    // WaveSource (stub)
    // WaveDetector (stub)
}
```

---

## Phase 1 vs Phase 2 Stubs

### Phase 1: Stubs (Empty Implementations)

```cpp
// components/Ball.h
class Ball : public BaseComponent {
public:
    void stepMotion(MotionContext&, double) override {}  // Empty
    // Other domains not used
};

// components/Mirror.h
class Mirror : public BaseComponent {
public:
    void traceRays(RayContext&) override {}  // Empty
    // Other domains not used
};
```

**Behavior**: 
- Can be placed on canvas
- Can be serialized/loaded
- Do nothing during simulation
- Placeholder for future implementation

### Phase 2: Full Implementation

When Phase 2 begins, stubs are replaced with real implementations:

```cpp
// components/Ball.cpp
void Ball::stepMotion(MotionContext& context, double dt) {
    // Calculate forces, integrate with RK4, update position
    // Actual physics implementation
}
```

---

## Creating a Component via Registry

### Example: User Drags Resistor from Parts Panel

```cpp
// In CanvasView::dropEvent(QDropEvent* event)

// 1. Extract type from MIME data
QString typeId = getMimeType(event->mimeData());  // "Resistor"

// 2. Ask factory to create instance
BaseComponent* component = ComponentFactory::createComponent(typeId);
if (!component) {
    QMessageBox::warning(this, "Error", "Unknown component type: " + typeId);
    return;
}

// 3. Set position
component->setPos(event->pos());

// 4. Add to scene (Qt owns it now)
scene->addItem(component);

// 5. Create command for undo/redo
auto* cmd = new CreateComponentCommand(component, scene, undoStack);
undoStack->push(cmd);
```

---

## Parts Panel: Querying the Registry

### Display Available Components

```cpp
// In PartsPanel constructor

auto components = ComponentFactory::allComponents();

for (const auto& info : components) {
    auto* item = new QListWidgetItem();
    item->setText(info.displayName);
    item->setIcon(info.icon);
    item->setData(Qt::UserRole, info.typeId);  // Store typeId for drag/drop
    
    listWidget->addItem(item);
}
```

### Drag from Parts Panel

```cpp
void PartsPanel::mousePressEvent(QMouseEvent* event) {
    auto* item = listWidget->itemAt(event->pos());
    if (!item) return;
    
    QString typeId = item->data(Qt::UserRole).toString();
    
    QMimeData* mimeData = new QMimeData();
    mimeData->setText(typeId);  // "Resistor", "Ball", etc.
    
    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(item->icon().pixmap(32, 32));
    
    drag->exec(Qt::CopyAction);
}
```

---

## Properties Panel: Property Editing

### Display Properties for Selected Component

```cpp
// In PropertiesPanel::onComponentSelected(BaseComponent* component)

clearLayout();

// Get component type info
auto info = ComponentFactory::getInfo(component->typeId);

for (const auto& propDef : info.properties) {
    // Create UI widget for each property
    QLabel* label = new QLabel(propDef.displayName + ":");
    QWidget* editor = createEditorForType(propDef.type, propDef);
    
    // Set current value
    QVariant currentValue = component->properties[propDef.name];
    setEditorValue(editor, currentValue);
    
    // Connect changes to EditPropertyCommand
    connect(editor, valueChangedSignal, [=](QVariant newValue) {
        auto* cmd = new EditPropertyCommand(
            component,
            propDef.name,
            currentValue,
            newValue
        );
        undoStack->push(cmd);
    });
    
    layout->addRow(label, editor);
}
```

### Editor Factory

```cpp
QWidget* createEditorForType(const QString& type, 
                            const PropertyDefinition& def) {
    if (type == "double") {
        auto* spin = new QDoubleSpinBox();
        spin->setMinimum(def.minValue.toDouble());
        spin->setMaximum(def.maxValue.toDouble());
        spin->setValue(def.defaultValue.toDouble());
        spin->setSuffix(" " + def.unit);
        return spin;
    } else if (type == "int") {
        auto* spin = new QSpinBox();
        spin->setMinimum(def.minValue.toInt());
        spin->setMaximum(def.maxValue.toInt());
        return spin;
    } else if (type == "string") {
        auto* line = new QLineEdit();
        line->setText(def.defaultValue.toString());
        return line;
    } else if (type == "bool") {
        auto* check = new QCheckBox();
        check->setChecked(def.defaultValue.toBool());
        return check;
    }
    return nullptr;
}
```

---

## Serialization Integration

### Save Component with Properties

```cpp
// ProjectDocument::saveComponent(BaseComponent* comp, QJsonObject& obj)

auto info = ComponentFactory::getInfo(comp->typeId);

obj["typeId"] = comp->typeId;
obj["displayName"] = comp->displayName;
obj["position"] = QJsonObject{
    {"x", comp->pos().x()},
    {"y", comp->pos().y()}
};

// Save all properties according to schema
QJsonObject propsObj;
for (const auto& propDef : info.properties) {
    QVariant value = comp->properties[propDef.name];
    propsObj[propDef.name] = QJsonValue::fromVariant(value);
}
obj["properties"] = propsObj;
```

### Load Component with Validation

```cpp
// ProjectDocument::loadComponent(const QJsonObject& obj)

QString typeId = obj["typeId"].toString();

// Validate type exists
if (!ComponentFactory::getInfo(typeId).creator) {
    throw std::runtime_error("Unknown component type: " + typeId.toStdString());
}

// Create component
BaseComponent* comp = ComponentFactory::createComponent(typeId);

// Load properties (validate against schema)
auto info = ComponentFactory::getInfo(typeId);
QJsonObject propsObj = obj["properties"].toObject();

for (const auto& propDef : info.properties) {
    if (propsObj.contains(propDef.name)) {
        QVariant value = propsObj[propDef.name].toVariant();
        // Could validate type, min/max here
        comp->properties[propDef.name] = value;
    } else {
        // Use default if not in file
        comp->properties[propDef.name] = propDef.defaultValue;
    }
}

return comp;
```

---

## Future: Plugin System

In Phase 5+, allow third-party component plugins:

```cpp
// Optional future enhancement

class ComponentPlugin {
public:
    virtual QList<ComponentInfo> providedComponents() = 0;
};

// At startup, scan plugin directory and load *.so/.dll files
void ComponentRegistry::loadPlugins(const QString& pluginDir) {
    // Iterate .so/.dll files
    // Load via QLibrary
    // Call providedComponents()
    // Register each component
}
```

---

## Summary: Phase 1 Component Checklist

### Core Components (Implement in Phase 1)

- [ ] Resistor (Electronics) — working MNA stamp
- [ ] VoltageSource (Electronics) — working MNA stamp
- [ ] CurrentSource (Electronics) — working MNA stamp
- [ ] Ammeter (Electronics) — measurement, no solution impact
- [ ] Voltmeter (Electronics) — measurement, no solution impact

### Stub Components (Phase 1, Implement Phase 2+)

- [ ] Capacitor (Electronics Phase 2)
- [ ] Inductor (Electronics Phase 2)
- [ ] Ball (Motion Phase 3)
- [ ] Block (Motion Phase 3)
- [ ] Spring (Motion Phase 3)
- [ ] Mirror (Optics Phase 4)
- [ ] Lens (Optics Phase 4)
- [ ] WaveSource (Wave Phase 4)
- [ ] WaveDetector (Wave Phase 4)

---

## Implementation Effort

- **Phase 1**: Component registration framework + 5 working components
  - Estimated: 8-10 hours
  - Includes: factory, parts panel UI, properties panel

- **Phase 2**: Add 2 dynamic components (Capacitor, Inductor)
  - Estimated: 4-6 hours
  - Plus 12-16 hours for MNA solver

- **Phase 3+**: Add motion, optics, wave components
  - Estimated: 4-6 hours per domain

Total Phase 1: ~18-20 hours (including UI, registration, 5 components)
