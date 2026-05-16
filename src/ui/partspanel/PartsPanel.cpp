#include "ui/partspanel/PartsPanel.h"

#include "components/BaseComponent.h"
#include "components/ComponentMimeTypes.h"
#include "components/ComponentRegistry.h"

#include <cmath>

#include <QAbstractItemView>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
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
        data->setData(ComponentMimeTypes::componentType, typeId.toUtf8());
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

// ---------------------------------------------------------------------------
// renderComponentIcon — creates a 48×48 icon by painting the component
// into an off-screen QGraphicsScene.  Returns a null QPixmap if the
// component can't be created.
// ---------------------------------------------------------------------------
static QIcon renderComponentIcon(const QString& typeId)
{
    constexpr int kSize = 48;

    BaseComponent* comp = ComponentRegistry::instance().create(typeId);
    if (!comp) return {};

    // Place component at scene centre so boundingRect doesn't go negative.
    comp->setPos(0, 0);

    QGraphicsScene tmpScene;
    tmpScene.addItem(comp);

    // Use the component's own bounding rect, padded 20 %.
    const QRectF br = comp->mapToScene(comp->boundingRect()).boundingRect();
    const double pad = std::max(br.width(), br.height()) * 0.20;
    const QRectF source = br.adjusted(-pad, -pad, pad, pad);

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    tmpScene.render(&painter, QRectF(0, 0, kSize, kSize), source);

    // comp is owned by tmpScene; it will be deleted when tmpScene goes out of scope.
    return QIcon(pixmap);
}

void PartsPanel::populate()
{
    tree->clear();

    // Show a larger icon + set icon size once.
    tree->setIconSize(QSize(40, 40));

    const QList<ComponentDescriptor> descriptors = ComponentRegistry::instance().descriptors();
    for (const ComponentDescriptor& descriptor : descriptors) {
        QTreeWidgetItem* category = categoryItem(descriptor.categoryPath);
        auto* item = new QTreeWidgetItem(category);
        item->setText(0, descriptor.displayName);
        item->setToolTip(0, descriptor.description.isEmpty()
                            ? descriptor.typeId
                            : descriptor.description);
        item->setData(0, typeIdRole, descriptor.typeId);
        item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);

        // Render the component's own paint() into a small icon.
        const QIcon icon = renderComponentIcon(descriptor.typeId);
        if (!icon.isNull())
            item->setIcon(0, icon);
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
