#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QString>

class ProjectDocument {
public:
    QString version = "1.0";
    QJsonArray components;

    QJsonObject toJson() const;
    static ProjectDocument fromJson(const QJsonObject& object);
};
