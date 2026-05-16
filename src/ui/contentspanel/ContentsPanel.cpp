#include "ui/contentspanel/ContentsPanel.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Curriculum definition — mirrors what the user dictated.
// Each entry is { "Display Name", "filename_without_extension" }.
// The full resource path is built as:
//   :/practicals/<prefix>/<filename>.pss
// ---------------------------------------------------------------------------
namespace {

// Helper to build a curriculum chapter entry.
QPair<QString,QString> P(const char* name, const char* file)
{
    return { QString::fromUtf8(name), QString::fromUtf8(file) };
}

struct Chapter {
    const char*                     title;
    const char*                     prefix;      // resource subdirectory
    QList<QPair<QString,QString>>   practicals;  // { display, filename }
};

// NOTE: these file names must match exactly what is baked into resources.qrc.
const Chapter kCurriculum[] = {
    {
        "1. Describing Motion",
        "describing_motion",
        {
            P("Distance-Time Graph",         "01_distance_time"),
            P("Velocity-Time Graph",         "02_velocity_time"),
            P("Acceleration",                "03_acceleration"),
        }
    },
    {
        "2. Force & Acceleration",
        "force_acceleration",
        {
            P("Circular Motion",             "01_circular_motion"),
            P("Circular Motion — Change Radius","02_circular_radius"),
            P("Circular Motion — Change Mass",  "03_circular_mass"),
            P("Newton's First Law",          "04_newton_first"),
            P("Newton's Second Law",         "05_newton_second"),
            P("Newton's Third Law",          "06_newton_third"),
            P("Resultant Force",             "07_resultant_force"),
            P("Stopping Distance",           "08_stopping_distance"),
            P("Traction",                    "09_traction"),
            P("Unequal Forces",              "10_unequal_forces"),
            P("Weight",                      "11_weight"),
            P("Balanced Forces",             "12_balanced_forces"),
            P("Defining the Newton",         "13_defining_newton"),
            P("Friction",                    "14_friction"),
            P("Pendulum",                    "15_pendulum"),
        }
    },
    {
        "3. Energy & Motion",
        "energy_motion",
        {
            P("Definition of Momentum",      "01_momentum_definition"),
            P("Conservation of Momentum",    "02_momentum_conservation"),
            P("Change in Momentum",          "03_momentum_change"),
            P("Kinetic Energy",              "04_kinetic_energy"),
            P("Changing Speed",              "05_changing_speed"),
            P("Change in Energy",            "06_energy_change"),
            P("Gravity & GPE",               "07_gravity_gpe"),
            P("Collisions",                  "08_collisions"),
            P("Elastic & Inelastic",         "09_elastic_inelastic"),
        }
    },
    {
        "4. Circuits",
        "circuits",
        {
            P("Basic Circuit",               "01_basic_circuit"),
            P("Current & Voltage",           "02_current_voltage"),
            P("LDR & Thermistor",            "03_ldr_thermistor"),
            P("Ohm's Law",                   "04_ohms_law"),
            P("Parallel Circuit",            "05_parallel_circuit"),
            P("Series Resistors",            "06_series_resistors"),
            P("Series Lamps",                "07_series_lamps"),
            P("Series Batteries",            "08_series_batteries"),
        }
    },
    {
        "5. Electrical Energy",
        "electrical_energy",
        {
            P("AC vs DC",                    "01_ac_dc"),
            P("Cost of Energy",              "02_cost_energy"),
            P("Electric Power",              "03_electric_power"),
            P("Fuse",                        "04_fuse"),
            P("High Power Appliances",       "05_high_power"),
            P("Low Power Appliances",        "06_low_power"),
            P("Transformer",                 "07_transformer"),
        }
    },
    {
        "6. Waves",
        "waves",
        {
            P("Absorption of Radiation",     "01_absorption"),
            P("Diffraction",                 "02_diffraction"),
            P("Doppler Effect",              "03_doppler_effect"),
            P("Interference",                "04_interference"),
            P("Loudness and Pitch",          "05_loudness_pitch"),
            P("Electromagnetic Waves",       "06_electromagnetic"),
            P("Speed of Sound",              "07_speed_sound"),
            P("Ultrasound",                  "08_ultrasound"),
        }
    },
    {
        "7. Optics",
        "optics",
        {
            P("Angle of Reflection",         "01_angle_reflection"),
            P("Convex Mirror",               "02_convex_mirror"),
            P("Mirror Reflection",           "03_mirror_reflection"),
            P("Camera Lens",                 "04_camera_lens"),
            P("Magnifying Glass",            "05_magnifying_glass"),
            P("Magnification",               "06_magnification"),
            P("Mirror Applications",         "07_mirror_applications"),
            P("Telescope",                   "08_telescope"),
        }
    },
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// ContentsPanel — constructor
// ---------------------------------------------------------------------------
ContentsPanel::ContentsPanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(200);

    // ── My Content path (~/Documents/PhysicsStudio/My Content/) ──────────────
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_myContentPath = docs + "/PhysicsStudio/My Content";
    QDir().mkpath(m_myContentPath);

    // ── Layout ────────────────────────────────────────────────────────────────
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Header label
    auto* header = new QLabel(tr("Contents"), this);
    QFont hf = header->font();
    hf.setBold(true);
    hf.setPointSize(hf.pointSize() + 1);
    header->setFont(hf);
    vbox->addWidget(header);

    // Search bar
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search practicals..."));
    m_search->setClearButtonEnabled(true);
    vbox->addWidget(m_search);

    // Tree
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setAnimated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);  // we handle double-click ourselves
    vbox->addWidget(m_tree);

    // ── Build the curriculum tree ─────────────────────────────────────────────
    buildTree();

    // Expand all chapters by default.
    m_tree->expandAll();

    // ── File system watcher for My Content ───────────────────────────────────
    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(m_myContentPath);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this,      &ContentsPanel::refreshMyContent);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_search, &QLineEdit::textChanged,
            this,     &ContentsPanel::filterItems);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this,   &ContentsPanel::onItemDoubleClicked);
}

// ---------------------------------------------------------------------------
// buildTree — populate with curriculum chapters + My Content
// ---------------------------------------------------------------------------
void ContentsPanel::buildTree()
{
    m_tree->clear();

    // Chapter font — bold top-level items
    QFont chapterFont = m_tree->font();
    chapterFont.setBold(true);

    for (const Chapter& ch : kCurriculum) {
        const QString prefix = QString::fromUtf8(ch.prefix);
        addChapter(QString::fromUtf8(ch.title), ch.practicals, prefix);
    }

    // My Content section
    m_myContentItem = addMyContentSection();
    refreshMyContent();
}

// ---------------------------------------------------------------------------
// addChapter — add one chapter node + its practical children
// ---------------------------------------------------------------------------
void ContentsPanel::addChapter(const QString& title,
                               const QList<QPair<QString,QString>>& practicals,
                               const QString& prefix)
{
    auto* chapter = new QTreeWidgetItem(m_tree);
    chapter->setText(0, title);
    QFont f = chapter->font(0);
    f.setBold(true);
    chapter->setFont(0, f);
    // Chapters are not selectable themselves (grey them slightly).
    chapter->setFlags(chapter->flags() & ~Qt::ItemIsSelectable);
    chapter->setForeground(0, QColor(60, 60, 60));

    for (const auto& [name, file] : practicals) {
        auto* item = new QTreeWidgetItem(chapter);
        item->setText(0, name);
        item->setData(0, kPathRole, QString(":/practicals/%1/%2.pss").arg(prefix, file));
        item->setData(0, kKindRole, QStringLiteral("builtin"));
        item->setToolTip(0, tr("Double-click to open: %1").arg(name));
        // Indent indicator with a small circle bullet
        item->setText(0, QStringLiteral("  ") + name);
    }
}

// ---------------------------------------------------------------------------
// addMyContentSection — returns the top-level "My Content" item
// ---------------------------------------------------------------------------
QTreeWidgetItem* ContentsPanel::addMyContentSection()
{
    auto* section = new QTreeWidgetItem(m_tree);
    section->setText(0, tr("My Content"));
    QFont f = section->font(0);
    f.setBold(true);
    section->setFont(0, f);
    section->setFlags(section->flags() & ~Qt::ItemIsSelectable);
    section->setForeground(0, QColor(40, 80, 140));
    return section;
}

// ---------------------------------------------------------------------------
// refreshMyContent — re-scan the My Content folder
// ---------------------------------------------------------------------------
void ContentsPanel::refreshMyContent()
{
    if (!m_myContentItem) return;

    // Remove all existing children.
    while (m_myContentItem->childCount() > 0)
        delete m_myContentItem->takeChild(0);

    QDir dir(m_myContentPath);
    const QStringList files = dir.entryList({"*.pss"}, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        auto* placeholder = new QTreeWidgetItem(m_myContentItem);
        placeholder->setText(0, tr("  (no saved practicals yet)"));
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable);
        placeholder->setForeground(0, Qt::gray);
    } else {
        for (const QString& fname : files) {
            const QString path = m_myContentPath + "/" + fname;
            auto* item = new QTreeWidgetItem(m_myContentItem);
            // Strip .pss extension for display
            QString display = QFileInfo(fname).completeBaseName();
            display.replace('_', ' ');
            item->setText(0, QStringLiteral("  ") + display);
            item->setData(0, kPathRole, path);
            item->setData(0, kKindRole, QStringLiteral("user"));
            item->setToolTip(0, path);
        }
    }

    m_myContentItem->setExpanded(true);
}

// ---------------------------------------------------------------------------
// onItemDoubleClicked — emit the appropriate signal
// ---------------------------------------------------------------------------
void ContentsPanel::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;

    const QString kind = item->data(0, kKindRole).toString();
    const QString path = item->data(0, kPathRole).toString();
    if (path.isEmpty()) return;

    if (kind == QLatin1String("builtin"))
        emit builtinPracticalRequested(path);
    else if (kind == QLatin1String("user"))
        emit userPracticalRequested(path);
}

// ---------------------------------------------------------------------------
// filterItems — show/hide children matching the search text
// ---------------------------------------------------------------------------
void ContentsPanel::filterItems(const QString& text)
{
    const QString lower = text.trimmed().toLower();

    // Iterate all top-level chapter items.
    for (int ci = 0; ci < m_tree->topLevelItemCount(); ++ci) {
        QTreeWidgetItem* chapter = m_tree->topLevelItem(ci);
        bool anyVisible = false;

        for (int pi = 0; pi < chapter->childCount(); ++pi) {
            QTreeWidgetItem* practical = chapter->child(pi);
            const bool matches = lower.isEmpty()
                || practical->text(0).toLower().contains(lower);
            practical->setHidden(!matches);
            if (matches) anyVisible = true;
        }

        // Hide the chapter header itself if nothing under it matches.
        chapter->setHidden(!anyVisible && !lower.isEmpty());

        // Always keep expanded when filtering.
        if (!lower.isEmpty() && anyVisible)
            chapter->setExpanded(true);
    }
}
