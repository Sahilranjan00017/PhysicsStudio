#include "components/BaseComponent.h"

#include "components/Wire.h"

#include <QJsonObject>

namespace {
constexpr auto idKey = "id";
constexpr auto typeIdKey = "typeId";
constexpr auto displayNameKey = "displayName";
constexpr auto positionKey = "position";
constexpr auto xKey = "x";
constexpr auto yKey = "y";
constexpr auto rotationKey = "rotation";
constexpr auto destructibleKey = "destructible";
constexpr auto destroyedKey = "destroyed";
constexpr auto lockedKey = "locked";
constexpr auto lockedPositionKey = "position";
constexpr auto lockedSizeKey = "size";
constexpr auto lockedOrientationKey = "orientation";
constexpr auto lockedPropertiesKey = "properties";
constexpr auto propertiesKey = "properties";
} // namespace

BaseComponent::BaseComponent(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

BaseComponent::~BaseComponent() = default;

void BaseComponent::setComponentRotation(double degrees)
{
    rotationDegrees = degrees;
    setRotation(degrees);
}

void BaseComponent::setComponentProperty(const QString& key, const QVariant& value)
{
    properties[key] = value;
    emit propertyChanged(key, value);
    update();
}

ConnectionPad* BaseComponent::padById(const QString& padId)
{
    for (ConnectionPad* pad : pads) {
        if (pad != nullptr && pad->padId == padId) {
            return pad;
        }
    }
    return nullptr;
}

const ConnectionPad* BaseComponent::padById(const QString& padId) const
{
    for (const ConnectionPad* pad : pads) {
        if (pad != nullptr && pad->padId == padId) {
            return pad;
        }
    }
    return nullptr;
}

void BaseComponent::stampMNA(double)
{
}

void BaseComponent::stepMotion(MotionContext&, double)
{
}

void BaseComponent::traceRays(RayContext&)
{
}

void BaseComponent::stepWave(WaveContext&, double)
{
}

QJsonObject BaseComponent::toJson() const
{
    QJsonObject object;
    object[idKey] = id;
    object[typeIdKey] = typeId;
    object[displayNameKey] = displayName;

    QJsonObject position;
    position[xKey] = pos().x();
    position[yKey] = pos().y();
    object[positionKey] = position;

    QJsonObject locked;
    locked[lockedPositionKey] = lockedPosition;
    locked[lockedSizeKey] = lockedSize;
    locked[lockedOrientationKey] = lockedOrientation;
    locked[lockedPropertiesKey] = lockedProperties;
    object[lockedKey] = locked;

    object[rotationKey] = rotationDegrees;
    object[destructibleKey] = destructible;
    object[destroyedKey] = destroyed;
    object[propertiesKey] = QJsonObject::fromVariantMap(properties);
    return object;
}

void BaseComponent::fromJson(const QJsonObject& object)
{
    id = object[idKey].toString();
    typeId = object[typeIdKey].toString();
    displayName = object[displayNameKey].toString();

    const QJsonObject position = object[positionKey].toObject();
    const double x = position.contains(xKey) ? position[xKey].toDouble() : object[xKey].toDouble();
    const double y = position.contains(yKey) ? position[yKey].toDouble() : object[yKey].toDouble();
    setPos(x, y);

    const QJsonObject locked = object[lockedKey].toObject();
    lockedPosition = locked[lockedPositionKey].toBool();
    lockedSize = locked[lockedSizeKey].toBool();
    lockedOrientation = locked[lockedOrientationKey].toBool();
    lockedProperties = locked[lockedPropertiesKey].toBool();

    setComponentRotation(object[rotationKey].toDouble());
    destructible = object[destructibleKey].toBool();
    destroyed = object[destroyedKey].toBool();
    properties = object[propertiesKey].toObject().toVariantMap();
}

QVariant BaseComponent::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemPositionHasChanged) {
        for (ConnectionPad* pad : pads) {
            if (pad == nullptr) {
                continue;
            }
            for (Wire* wire : pad->connectedWires) {
                if (wire != nullptr) {
                    wire->reroute();
                }
            }
        }
    }

    return QGraphicsObject::itemChange(change, value);
}
