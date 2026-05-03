#include "components/BaseComponent.h"

#include <QJsonObject>

BaseComponent::BaseComponent(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
}

BaseComponent::~BaseComponent() = default;

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
    object["id"] = id;
    object["typeId"] = typeId;
    object["displayName"] = displayName;
    object["x"] = pos().x();
    object["y"] = pos().y();
    object["rotation"] = rotationDegrees;
    object["destructible"] = destructible;
    object["destroyed"] = destroyed;
    return object;
}

void BaseComponent::fromJson(const QJsonObject& object)
{
    id = object["id"].toString();
    typeId = object["typeId"].toString();
    displayName = object["displayName"].toString();
    setPos(object["x"].toDouble(), object["y"].toDouble());
    rotationDegrees = object["rotation"].toDouble();
    destructible = object["destructible"].toBool();
    destroyed = object["destroyed"].toBool();
}

