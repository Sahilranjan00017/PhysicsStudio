#pragma once

#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class PartsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PartsPanel(QWidget* parent = nullptr);

private slots:
    void filterParts(const QString& text);

private:
    void populate();
    QTreeWidgetItem* categoryItem(const QString& categoryPath);
    void updateItemVisibility(QTreeWidgetItem* item, const QString& filter);

    QLineEdit* searchBox = nullptr;
    QTreeWidget* tree = nullptr;
};

