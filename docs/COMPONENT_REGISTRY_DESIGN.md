# Component Registry Design

The component registry is the bridge between the parts panel, JSON persistence, and concrete C++ component classes.

## Responsibilities

- Map `typeId` to a component factory.
- Provide display metadata for the parts panel.
- Provide default editable properties.
- Provide pad definitions.
- Support deserialization from `.pss` files.

## Planned API

```cpp
struct ComponentDescriptor {
    QString typeId;
    QString displayName;
    QString categoryPath;
    QString iconPath;
    QJsonObject defaultProperties;
    QList<PadDescriptor> pads;
};

class ComponentRegistry {
public:
    void registerType(ComponentDescriptor descriptor, Factory factory);
    BaseComponent* create(const QString& typeId) const;
    QList<ComponentDescriptor> descriptors() const;
};
```

## Phase 1 Entries

Electronics:

- `ELEC_GND`: Ground
- `ELEC_RES`: Resistor
- `ELEC_VSRC`: DC Voltage Source
- `ELEC_ISRC`: DC Current Source
- `ELEC_AMM`: Ammeter
- `ELEC_VOLTM`: Voltmeter

Motion stubs:

- `MOT_BALL`: Ball
- `MOT_BLOCK`: Block
- `MOT_SPRING`: Spring

Optics stubs:

- `OPT_SPACE`: Optical Space
- `OPT_MIRROR_P`: Plane Mirror

Waves stubs:

- `WAV_1D`: 1D Wave Space
- `WAV_PT`: Point Source

## Parts Panel Population

```text
ComponentRegistry::descriptors()
  -> group by categoryPath
  -> create QTreeWidget category nodes
  -> attach typeId to each leaf item
  -> drag operation serializes typeId in QMimeData
```

## Persistence Flow

```text
ProjectDocument loads component JSON
  -> read typeId
  -> registry creates matching component
  -> BaseComponent::fromJson applies common fields
  -> subclass applies domain-specific properties if needed
```

Unknown `typeId` values should create a placeholder component rather than losing file data. This allows older/newer `.pss` files to open with warnings.

