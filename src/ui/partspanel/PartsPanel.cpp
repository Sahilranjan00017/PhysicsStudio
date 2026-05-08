#include "ui/partspanel/PartsPanel.h"

#include "components/ComponentRegistry.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLineEdit>
#include <QMimeData>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr auto componentTypeMime = "application/x-physicsstudio-component-type";
constexpr int typeIdRole = Qt::UserRole + 1;

class PartsTreeWidget final : public QTreeWidget {
public:
    explicit PartsTreeWidget(QWidget* parent = nullptr)
        : QTreeWidget(parent)
    {
    }

protected:
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override
    {
        if (items.isEmpty()) {
            return nullptr;
        }

        const QString typeId = items.first()->data(0, typeIdRole).toString();
        if (typeId.isEmpty()) {
            return nullptr;
        }

        auto* data = new QMimeData();
        data->setData(componentTypeMime, typeId.toUtf8());
        data->setText(typeId);
        return data;
    }
};
} // namespace

PartsPanel::PartsPanel(QWidget* parent)
    : QWidget(parent),
      searchBox(new QLineEdit(this)),
      tree(new PartsTreeWidget(this))
{
    searchBox->setPlaceholderText("Search parts");

    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    tree->setDragEnabled(true);
    tree->setDragDropMode(QAbstractItemView::DragOnly);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->header()->setStretchLastSection(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(searchBox);
    layout->addWidget(tree);

    populate();
    connect(searchBox, &QLineEdit::textChanged, this, &PartsPanel::filterParts);
}

void PartsPanel::filterParts(const QString& text)
{
    const QString filter = text.trimmed().toLower();
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        updateItemVisibility(tree->topLevelItem(i), filter);
    }
}

void PartsPanel::populate()
{
    tree->clear();

    const QList<ComponentDescriptor> descriptors = ComponentRegistry::instance().descriptors();
    for (const ComponentDescriptor& descriptor : descriptors) {
        QTreeWidgetItem* category = categoryItem(descriptor.categoryPath);
        auto* item = new QTreeWidgetItem(category);
        item->setText(0, descriptor.displayName);
        item->setToolTip(0, descriptor.description);
        item->setData(0, typeIdRole, descriptor.typeId);
        item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
    }

    tree->expandAll();
}

QTreeWidgetItem* PartsPanel::categoryItem(const QString& categoryPath)
{
    const QStringList parts = categoryPath.split('/', Qt::SkipEmptyParts);
    QTreeWidgetItem* parent = nullptr;

    for (const QString& part : parts) {
        QTreeWidgetItem* found = nullptr;
        const int childCount = parent == nullptr ? tree->topLevelItemCount() : parent->childCount();
        for (int i = 0; i < childCount; ++i) {
            QTreeWidgetItem* candidate = parent == nullptr ? tree->topLevelItem(i) : parent->child(i);
            if (candidate->text(0) == part && candidate->data(0, typeIdRole).toString().isEmpty()) {
                found = candidate;
                break;
            }
        }

        if (found == nullptr) {
            found = parent == nullptr ? new QTreeWidgetItem(tree) : new QTreeWidgetItem(parent);
            found->setText(0, part);
            found->setFlags((found->flags() & ~Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
        }
        parent = found;
    }

    return parent;
}

void PartsPanel::updateItemVisibility(QTreeWidgetItem* item, const QString& filter)
{
    bool childVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        updateItemVisibility(item->child(i), filter);
        childVisible = childVisible || !item->child(i)->isHidden();
    }

    const QString itemText = item->text(0).toLower();
    const QString typeId = item->data(0, typeIdRole).toString().toLower();
    const bool selfVisible = filter.isEmpty() || itemText.contains(filter) || typeId.contains(filter);
    item->setHidden(!selfVisible && !childVisible);
}
