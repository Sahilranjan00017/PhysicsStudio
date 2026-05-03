#pragma once

#include <QList>
#include <QPointF>
#include <QString>

class Wire;

enum class PadType {
    Input,
    Output,
    Bidirectional
};

enum class DomainType {
    Electrical,
    Mechanical,
    Optical,
    Wave
};

struct ConnectionPad {
    QPointF localPos;
    PadType type = PadType::Bidirectional;
    DomainType domain = DomainType::Electrical;
    QString padId;
    QList<Wire*> connectedWires;
    bool highlighted = false;
};

