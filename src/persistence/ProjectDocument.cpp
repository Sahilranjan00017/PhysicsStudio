#include "persistence/ProjectDocument.h"

QJsonObject ProjectDocument::toJson() const
{
    QJsonObject object;
    object["pss_version"] = version;
    object["components"] = components;
    object["wires"] = wires;
    return object;
}

ProjectDocument ProjectDocument::fromJson(const QJsonObject& object)
{
    ProjectDocument document;
    document.version = object["pss_version"].toString("1.0");
    document.components = object["components"].toArray();
    document.wires = object["wires"].toArray();
    return document;
}
