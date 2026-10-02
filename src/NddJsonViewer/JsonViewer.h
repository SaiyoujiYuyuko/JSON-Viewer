#pragma once

#include "EditorBridge.h"
#include "OrderedJson.h"
#include <QDockWidget>
#include <QSettings>
#include <QTreeWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QAction>
#include <memory>

namespace ndd
{
class JsonViewer : public QObject
{
    EditorBridge editor;
    Snapshot snapshot;
    jsonviewer::Document document;
    QPointer<QWidget> host;
    QDockWidget* dock = nullptr;
    QTreeWidget* tree = nullptr;
    QLabel* status = nullptr;
    QLineEdit* search = nullptr;
    QAction* follow = nullptr;
    QTimer* editTimer = nullptr;
    QMetaObject::Connection textConnection;
    std::unique_ptr<QSettings> settings;
    bool selectionMode = false;
    bool refreshing = false;

    ParseOptions parseOptions() const;
    void refresh(bool selection);
    void populate(QTreeWidgetItem* parent, const jsonviewer::Node& node);
    void navigate(QTreeWidgetItem* item, bool selectKey);
    void findNext();
    void contextMenu(const QPoint& point);
    void transform(bool compress, bool sort);
    void configure();
    void setZoom(int percent);
    void report(const QString& text, bool error = false);
public:
    JsonViewer(QWidget* host, NddGetEditor getEditor, const QString& pluginFile, QMenu* menu);
    ~JsonViewer() override;
    void show(bool selection = false);
};
}
