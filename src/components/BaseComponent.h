#pragma once

#include "components/ConnectionPad.h"

#include <QGraphicsObject>
#include <QJsonObject>
#include <QMap>
#include <QVariant>

class MotionContext;
class RayContext;
class WaveContext;

class BaseComponent : public QGraphicsObject {
    Q_OBJECT

public:
    explicit BaseComponent(QGraphicsItem* parent = nullptr);
    ~BaseComponent() override;

    QString id;
    QString typeId;
    QString displayName;
    double rotationDegrees = 0.0;
    bool destructible = false;
    bool destroyed = false;
    bool lockedPosition = false;
    bool lockedSize = false;
    bool lockedOrientation = false;
    bool lockedProperties = false;

    QMap<QString, QVariant> properties;
    QMap<QString, QVariant> simState;
    // Non-owning pad links; concrete components own pad storage until a scene model exists.
    QList<ConnectionPad*> pads;

    void setComponentRotation(double degrees);

    virtual void stampMNA(double dt);
    virtual void stepMotion(MotionContext& context, double dt);
    virtual void traceRays(RayContext& context);
    virtual void stepWave(WaveContext& context, double dt);

    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& object);

signals:
    void propertyChanged(QString key, QVariant value);
    void destroyedSignal(BaseComponent* component);
    void repairedSignal(BaseComponent* component);
};
