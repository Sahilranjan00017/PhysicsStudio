#pragma once

#include "components/BaseComponent.h"

#include <QRectF>

#include <array>

class ComponentRegistry;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class TwoTerminalElectricalComponent : public BaseComponent {
    Q_OBJECT

public:
    explicit TwoTerminalElectricalComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;

protected:
    void drawTerminals(QPainter* painter) const;
    void drawLabel(QPainter* painter) const;

private:
    std::array<ConnectionPad, 2> padStorage;
};

class ResistorComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit ResistorComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class VoltageSourceComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit VoltageSourceComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class CurrentSourceComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit CurrentSourceComponent(QGraphicsItem* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class AmmeterComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit AmmeterComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class VoltmeterComponent final : public TwoTerminalElectricalComponent {
    Q_OBJECT

public:
    explicit VoltmeterComponent(QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};

class GroundComponent final : public BaseComponent {
    Q_OBJECT

public:
    explicit GroundComponent(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    ConnectionPad groundPad;
};

void registerElectronicsComponents(ComponentRegistry& registry);
