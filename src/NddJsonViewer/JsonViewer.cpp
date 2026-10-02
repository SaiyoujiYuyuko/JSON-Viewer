#include "JsonViewer.h"
#include "JsonHandler.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMainWindow>
#include <QMenu>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QToolBar>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <algorithm>

namespace ndd
{
namespace
{
QString utf8(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<int>(value.size())); }
const jsonviewer::Node* nodeFor(QTreeWidgetItem* item)
{
    return item ? reinterpret_cast<const jsonviewer::Node*>(item->data(0, Qt::UserRole).value<quintptr>()) : nullptr;
}
QString labelFor(const jsonviewer::Node& node)
{
    auto key = !node.parent ? QStringLiteral("JSON") : node.parent->type == jsonviewer::Type::Array
        ? QStringLiteral("[%1]").arg(node.index) : node.key.empty() ? QStringLiteral("\"\"") : utf8(node.key);
    QString value;
    if (node.type == jsonviewer::Type::Array) value = QStringLiteral("[%1]").arg(node.children.size());
    else if (node.type == jsonviewer::Type::Object) value = QStringLiteral("{%1}").arg(node.children.size());
    else if (node.type == jsonviewer::Type::String) value = utf8(jsonviewer::quote(node.value));
    else value = utf8(node.value);
    if (value.size() > 512) value = value.left(512) + QStringLiteral("…");
    return key + QStringLiteral(" : ") + value;
}
}

JsonViewer::JsonViewer(QWidget* window, NddGetEditor getEditor, const QString& pluginFile, QMenu* menu)
    : QObject(window), editor(window, std::move(getEditor)), host(window)
{
    setObjectName(QStringLiteral("jsonViewerUpstreamPlugin"));
    auto configDir = QFileInfo(pluginFile).absoluteDir();
    configDir.mkpath(QStringLiteral("config"));
    settings = std::make_unique<QSettings>(configDir.filePath(QStringLiteral("config/JSONViewer.Upstream.ini")), QSettings::IniFormat);
    dock = new QDockWidget(QStringLiteral("JSON Viewer — source order"), window);
    dock->setObjectName(QStringLiteral("jsonViewerUpstreamDock"));
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setMinimumWidth(260);
    auto* body = new QWidget(dock);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(5, 4, 5, 4);
    layout->setSpacing(4);
    auto* toolbar = new QToolBar(body);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    auto* refreshAction = toolbar->addAction(QIcon(QStringLiteral(":/jsonviewer/refresh.ico")), QStringLiteral("Refresh JSON tree"));
    connect(refreshAction, &QAction::triggered, this, [this] { refresh(selectionMode); });
    auto* validateAction = toolbar->addAction(QIcon(QStringLiteral(":/jsonviewer/validate.ico")), QStringLiteral("Validate JSON"));
    connect(validateAction, &QAction::triggered, this, [this] {
        refresh(selectionMode);
        if (document) report(QStringLiteral("Valid JSON. Fields and elements follow source order."));
    });
    auto* formatAction = toolbar->addAction(QIcon(QStringLiteral(":/jsonviewer/format.ico")), QStringLiteral("Format JSON (undoable)"));
    connect(formatAction, &QAction::triggered, this, [this] { transform(false, false); });
    toolbar->addSeparator();
    auto* expand = toolbar->addAction(body->style()->standardIcon(QStyle::SP_ArrowDown), QStringLiteral("Expand all"));
    auto* collapse = toolbar->addAction(body->style()->standardIcon(QStyle::SP_ArrowUp), QStringLiteral("Collapse all"));
    follow = toolbar->addAction(body->style()->standardIcon(QStyle::SP_FileDialogContentsView), QStringLiteral("Follow active document"));
    follow->setCheckable(true);
    follow->setChecked(settings->value(QStringLiteral("FollowDocument"), true).toBool());
    connect(follow, &QAction::toggled, this, [this](bool value) { settings->setValue(QStringLiteral("FollowDocument"), value); });
    layout->addWidget(toolbar);
    search = new QLineEdit(body);
    search->setPlaceholderText(QStringLiteral("Search keys or values — Enter for next"));
    search->setClearButtonEnabled(true);
    connect(search, &QLineEdit::returnPressed, this, &JsonViewer::findNext);
    layout->addWidget(search);
    tree = new QTreeWidget(body);
    tree->setObjectName(QStringLiteral("jsonViewerSourceTree"));
    tree->setColumnCount(1);
    tree->setHeaderHidden(true);
    tree->setSortingEnabled(false);
    tree->setUniformRowHeights(true);
    tree->setIndentation(18);
    tree->setAnimated(false);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(false);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QTreeWidget::customContextMenuRequested, this, &JsonViewer::contextMenu);
    connect(tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) { navigate(item, false); });
    connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) { navigate(item, true); });
    connect(expand, &QAction::triggered, tree, &QTreeWidget::expandAll);
    connect(collapse, &QAction::triggered, this, [this] { tree->collapseAll(); if (tree->topLevelItemCount()) tree->topLevelItem(0)->setExpanded(true); });
    layout->addWidget(tree, 1);
    status = new QLabel(body);
    status->setWordWrap(true);
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    status->setMinimumHeight(32);
    layout->addWidget(status);
    auto* zoomRow = new QHBoxLayout;
    auto* zoom = new QSlider(Qt::Horizontal, body);
    zoom->setRange(80, 250);
    zoom->setValue(std::clamp(settings->value(QStringLiteral("Zoom"), 100).toInt(), 80, 250));
    zoom->setToolTip(QStringLiteral("Tree font size"));
    auto* zoomLabel = new QLabel(body);
    zoomRow->addWidget(new QLabel(QStringLiteral("Zoom"), body));
    zoomRow->addWidget(zoom, 1);
    zoomRow->addWidget(zoomLabel);
    connect(zoom, &QSlider::valueChanged, this, [this, zoomLabel](int value) {
        setZoom(value); zoomLabel->setText(QString::number(value) + QLatin1Char('%'));
    });
    setZoom(zoom->value());
    zoomLabel->setText(QString::number(zoom->value()) + QLatin1Char('%'));
    layout->addLayout(zoomRow);
    dock->setWidget(body);
    if (auto* mainWindow = qobject_cast<QMainWindow*>(window))
        mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);
    dock->hide();

    menu->addAction(QStringLiteral("Show JSON Viewer"), this, [this] { show(false); }, QKeySequence(QStringLiteral("Ctrl+Alt+Shift+J")));
    menu->addAction(QStringLiteral("View selected JSON"), this, [this] { show(true); });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Format JSON"), this, [this] { transform(false, false); });
    menu->addAction(QStringLiteral("Compress JSON"), this, [this] { transform(true, false); });
    menu->addAction(QStringLiteral("Sort keys (changes document)"), this, [this] { transform(false, true); });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Settings"), this, &JsonViewer::configure);
    menu->addAction(QStringLiteral("About"), this, [this] {
        dock->show();
        report(QStringLiteral("JSON Viewer for Notepad-- 1.0.0\nShares the current upstream JSON formatter and RapidJSON fork. Tree fields always follow source order."));
    });

    editTimer = new QTimer(this);
    editTimer->setSingleShot(true);
    editTimer->setInterval(400);
    connect(editTimer, &QTimer::timeout, this, [this] {
        if (!dock->isVisible()) return;
        if (snapshot.selection) report(QStringLiteral("Document changed. Refresh the selected JSON to rebuild its tree."), true);
        else refresh(false);
    });
    auto* tabTimer = new QTimer(this);
    tabTimer->setInterval(400);
    connect(tabTimer, &QTimer::timeout, this, [this] {
        if (dock->isVisible() && follow->isChecked() && editor.current() != snapshot.editor)
        {
            selectionMode = false;
            refresh(false);
        }
    });
    tabTimer->start();
}

JsonViewer::~JsonViewer()
{
    disconnect(textConnection);
    settings->sync();
}

ParseOptions JsonViewer::parseOptions() const
{
    ParseOptions options;
    options.bIgnoreComment = settings->value(QStringLiteral("AllowComments"), true).toBool();
    options.bIgnoreTrailingComma = settings->value(QStringLiteral("AllowTrailingComma"), true).toBool();
    return options;
}

void JsonViewer::show(bool selection)
{
    selectionMode = selection;
    dock->show();
    dock->raise();
    refresh(selection);
}

void JsonViewer::refresh(bool selection)
{
    if (refreshing) return;
    refreshing = true;
    editTimer->stop();
    disconnect(textConnection);
    QSignalBlocker blocker(tree);
    tree->clear();
    document = {};
    QString error;
    if (!editor.capture(snapshot, selection, error))
        report(error, true);
    else
    {
        textConnection = connect(snapshot.editor, &QsciScintilla::textChanged, this, [this] { editTimer->start(); });
        document = jsonviewer::parse(snapshot.json.toStdString(), parseOptions());
        if (document)
        {
            populate(nullptr, *document.root);
            tree->topLevelItem(0)->setExpanded(true);
            report(snapshot.selection ? QStringLiteral("Selected JSON · source order") : QStringLiteral("Source order · [n] array elements · {n} object fields"));
        }
        else report(QStringLiteral("JSON error at byte %1: %2").arg(document.errorOffset).arg(utf8(document.error)), true);
    }
    refreshing = false;
}

void JsonViewer::populate(QTreeWidgetItem* parent, const jsonviewer::Node& node)
{
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
    item->setText(0, labelFor(node));
    item->setData(0, Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(&node)));
    item->setToolTip(0, utf8(node.path()));
    if (node.isContainer())
    {
        auto font = tree->font();
        font.setWeight(QFont::DemiBold);
        item->setFont(0, font);
    }
    for (const auto& child : node.children) populate(item, *child);
}

void JsonViewer::navigate(QTreeWidgetItem* item, bool selectKey)
{
    const auto* node = nodeFor(item);
    if (!node) return;
    const bool hasKey = node->parent && node->parent->type == jsonviewer::Type::Object;
    const auto begin = hasKey ? node->keyBegin : node->valueBegin;
    const auto end = selectKey ? (hasKey ? node->keyEnd : node->valueEnd) : begin;
    if (editor.navigate(snapshot, begin, end)) report(utf8(node->path()));
    else report(QStringLiteral("Document changed or another tab is active. Refresh before navigating."), true);
}

void JsonViewer::findNext()
{
    if (search->text().isEmpty()) return;
    std::vector<QTreeWidgetItem*> items;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) items.push_back(*it);
    if (items.empty()) return;
    const auto current = std::find(items.begin(), items.end(), tree->currentItem());
    const std::size_t start = current == items.end() ? 0 : (current - items.begin() + 1) % items.size();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        auto* item = items[(start + i) % items.size()];
        const auto* node = nodeFor(item);
        if (node && (utf8(node->key).contains(search->text(), Qt::CaseInsensitive) || utf8(node->value).contains(search->text(), Qt::CaseInsensitive)))
        {
            for (auto* parent = item->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
            tree->setCurrentItem(item);
            tree->scrollToItem(item);
            return;
        }
    }
    report(QStringLiteral("No match: ") + search->text());
}

void JsonViewer::contextMenu(const QPoint& point)
{
    auto* item = tree->itemAt(point);
    const auto* node = nodeFor(item);
    if (!node) return;
    QMenu menu(tree);
    auto* copyName = menu.addAction(QStringLiteral("Copy name"));
    auto* copyValue = menu.addAction(QStringLiteral("Copy value as JSON"));
    auto* copyPath = menu.addAction(QStringLiteral("Copy path"));
    menu.addSeparator();
    auto* expand = menu.addAction(QStringLiteral("Expand subtree"));
    auto* collapse = menu.addAction(QStringLiteral("Collapse subtree"));
    // Keep the snapshot stable while the context menu runs its nested event loop.
    const auto name = node->parent && node->parent->type == jsonviewer::Type::Array ? QString::number(node->index) : utf8(node->key);
    const auto value = utf8(document.rawValue(*node));
    const auto path = utf8(node->path());
    const QSignalBlocker blockEdits(editTimer);
    const bool wasRefreshing = refreshing;
    refreshing = true;
    auto* chosen = menu.exec(tree->viewport()->mapToGlobal(point));
    if (chosen == copyName) QApplication::clipboard()->setText(name);
    else if (chosen == copyValue) QApplication::clipboard()->setText(value);
    else if (chosen == copyPath) QApplication::clipboard()->setText(path);
    else if (chosen == expand || chosen == collapse)
    {
        std::vector<QTreeWidgetItem*> pending{item};
        while (!pending.empty())
        {
            auto* next = pending.back(); pending.pop_back();
            next->setExpanded(chosen == expand);
            for (int i = 0; i < next->childCount(); ++i) pending.push_back(next->child(i));
        }
    }
    refreshing = wasRefreshing;
}

void JsonViewer::transform(bool compress, bool sort)
{
    Snapshot input;
    QString error;
    dock->show();
    if (!editor.capture(input, true, error)) { report(error, true); return; }
    // Validate through the ordered parser first (also rejects embedded NUL and
    // excessive nesting) before invoking the shared upstream formatter.
    auto parsed = jsonviewer::parse(input.json.toStdString(), parseOptions());
    if (!parsed) { report(utf8(parsed.error), true); return; }
    if (sort)
    {
        const auto problem = jsonviewer::keySortError(*parsed.root);
        if (!problem.empty()) { report(utf8(problem), true); return; }
    }
    JsonHandler handler(parseOptions());
    const auto eol = EditorBridge::send(input.editor, QsciScintillaBase::SCI_GETEOLMODE);
    const auto lineEnding = eol == 0 ? LE::kCrLf : eol == 1 ? LE::kCr : LE::kLf;
    const bool tabs = EditorBridge::send(input.editor, QsciScintillaBase::SCI_GETUSETABS) != 0;
    const auto width = EditorBridge::send(input.editor, QsciScintillaBase::SCI_GETINDENT);
    const unsigned indent = tabs ? 1 : static_cast<unsigned>(std::clamp<std::intptr_t>(width, 1, 16));
    const auto source = input.json.toStdString();
    auto result = compress ? handler.GetCompressedJson(source) : sort
        ? handler.SortJsonByKey(source, lineEnding, LF::kFormatDefault, tabs ? '\t' : ' ', indent)
        : handler.FormatJson(source, lineEnding, LF::kFormatDefault, tabs ? '\t' : ' ', indent);
    if (!result.success) { report(utf8(result.error_str), true); return; }
    if (!editor.replace(input, QByteArray(result.response.data(), static_cast<int>(result.response.size())), error))
    { report(error, true); return; }
    selectionMode = input.selection;
    refresh(selectionMode);
}

void JsonViewer::configure()
{
    QDialog dialog(host);
    dialog.setWindowTitle(QStringLiteral("JSON Viewer settings"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* comments = new QCheckBox(QStringLiteral("Allow comments when parsing"), &dialog);
    auto* commas = new QCheckBox(QStringLiteral("Allow trailing commas"), &dialog);
    comments->setChecked(parseOptions().bIgnoreComment);
    commas->setChecked(parseOptions().bIgnoreTrailingComma);
    layout->addWidget(comments);
    layout->addWidget(commas);
    auto* note = new QLabel(QStringLiteral("Tree fields always follow source order.\nFormatting uses the editor's indentation and line endings.\nFormatting/compression removes comments, as in upstream."), &dialog);
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted)
    {
        settings->setValue(QStringLiteral("AllowComments"), comments->isChecked());
        settings->setValue(QStringLiteral("AllowTrailingComma"), commas->isChecked());
        if (dock->isVisible()) refresh(selectionMode);
    }
}

void JsonViewer::setZoom(int percent)
{
    auto font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSizeF(10.0 * percent / 100.0);
    tree->setFont(font);
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
    {
        auto itemFont = font;
        if (const auto* node = nodeFor(*it); node && node->isContainer()) itemFont.setWeight(QFont::DemiBold);
        (*it)->setFont(0, itemFont);
    }
    settings->setValue(QStringLiteral("Zoom"), percent);
}

void JsonViewer::report(const QString& text, bool error)
{
    status->setText(text);
    auto palette = status->palette();
    palette.setColor(QPalette::WindowText, error ? QColor(190, 65, 45) : tree->palette().color(QPalette::Text));
    status->setPalette(palette);
}
}
