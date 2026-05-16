#pragma once

#include <QWidget>
#include <QString>

class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

// ---------------------------------------------------------------------------
// ContentsPanel — Crocodile Physics-style curriculum browser
//
// Shows a two-level tree:
//   ▶ Chapter 1: Describing Motion
//       Distance-Time Graph
//       Velocity-Time Graph
//       ...
//   ▶ My Content
//       (user-saved .pss files from Documents/PhysicsStudio/My Content/)
//
// Signals:
//   builtinPracticalRequested(resourcePath)   — load from Qt resource
//   userPracticalRequested(filePath)          — load from local filesystem
// ---------------------------------------------------------------------------

class ContentsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ContentsPanel(QWidget* parent = nullptr);
    ~ContentsPanel() override = default;

signals:
    void builtinPracticalRequested(const QString& resourcePath);
    void userPracticalRequested(const QString& filePath);

public slots:
    // Re-scan the user's My Content folder and rebuild that section.
    void refreshMyContent();

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void filterItems(const QString& text);

private:
    void buildTree();
    void addChapter(const QString& title, const QList<QPair<QString,QString>>& practicals,
                    const QString& resourcePrefix);
    QTreeWidgetItem* addMyContentSection();

    QLineEdit*         m_search        = nullptr;
    QTreeWidget*       m_tree          = nullptr;
    QTreeWidgetItem*   m_myContentItem = nullptr;
    QFileSystemWatcher* m_watcher      = nullptr;
    QString            m_myContentPath;

    // Role for storing the payload in a tree item.
    // Qt::UserRole+0 = resource path (built-in) or file path (My Content)
    // Qt::UserRole+1 = "builtin" | "user"
    static constexpr int kPathRole = Qt::UserRole;
    static constexpr int kKindRole = Qt::UserRole + 1;
};
