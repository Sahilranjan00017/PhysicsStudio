#pragma once

#include <QJsonObject>
#include <QString>

class ProjectDocument {
public:
    QString version = "1.0";
    QJsonObject toJson() const;
    static ProjectDocument fromJson(const QJsonObject& object);
};

