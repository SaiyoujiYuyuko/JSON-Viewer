#include "EditorBridge.h"
#include <algorithm>
#include <utility>

namespace ndd
{
using Sci = QsciScintillaBase;
using DirectFunction = std::intptr_t (*)(std::intptr_t, unsigned, std::uintptr_t, std::intptr_t);

EditorBridge::EditorBridge(QWidget* window, NddGetEditor callback)
    : host(window), getEditor(std::move(callback)) {}

QsciScintilla* EditorBridge::current() const
{
    return host && getEditor ? getEditor(host) : nullptr;
}

std::intptr_t EditorBridge::send(QsciScintilla* editor, unsigned message,
                               std::uintptr_t wParam, std::intptr_t lParam)
{
    if (!editor) return 0;
    // SendScintilla's long return is only 32 bits on Windows. Obtain the
    // documented pointer-sized direct interface instead of truncating pointers.
    auto function = reinterpret_cast<DirectFunction>(editor->SendScintillaPtrResult(Sci::SCI_GETDIRECTFUNCTION));
    auto pointer = reinterpret_cast<std::intptr_t>(editor->SendScintillaPtrResult(Sci::SCI_GETDIRECTPOINTER));
    return function && pointer ? function(pointer, message, wParam, lParam) : 0;
}

static bool read(QsciScintilla* editor, QByteArray& text)
{
    constexpr std::intptr_t maxBytes = 64 * 1024 * 1024;
    const auto length = EditorBridge::send(editor, Sci::SCI_GETLENGTH);
    if (length < 0 || length > maxBytes) return false;
    text.resize(static_cast<int>(length + 1));
    EditorBridge::send(editor, Sci::SCI_GETTEXT, text.size(), reinterpret_cast<std::intptr_t>(text.data()));
    text.resize(static_cast<int>(length));
    return true;
}

bool EditorBridge::capture(Snapshot& snapshot, bool selected, QString& error) const
{
    snapshot = {};
    auto* editor = current();
    if (!editor) { error = QStringLiteral("No active text editor."); return false; }
    if (send(editor, Sci::SCI_GETCODEPAGE) != 65001)
    {
        error = QStringLiteral("The active editor must use UTF-8 internally for exact JSON navigation.");
        return false;
    }
    if (selected && send(editor, Sci::SCI_GETSELECTIONS) > 1)
    {
        error = QStringLiteral("Select one JSON fragment; multiple selections are not supported.");
        return false;
    }
    if (!read(editor, snapshot.fullText))
    {
        error = QStringLiteral("This viewer supports buffers up to 64 MiB.");
        return false;
    }
    snapshot.editor = editor;
    snapshot.end = snapshot.fullText.size();
    if (selected)
    {
        const auto start = send(editor, Sci::SCI_GETSELECTIONSTART);
        const auto end = send(editor, Sci::SCI_GETSELECTIONEND);
        if (start != end)
        {
            snapshot.start = std::min(start, end);
            snapshot.end = std::max(start, end);
            snapshot.selection = true;
        }
    }
    snapshot.json = snapshot.fullText.mid(static_cast<int>(snapshot.start), static_cast<int>(snapshot.end - snapshot.start));
    // A UTF-8 BOM is metadata, not JSON. Keep editor byte offsets accurate.
    if (snapshot.json.startsWith("\xef\xbb\xbf"))
    {
        snapshot.json.remove(0, 3);
        snapshot.start += 3;
    }
    return true;
}

bool EditorBridge::unchanged(const Snapshot& snapshot) const
{
    QByteArray text;
    return snapshot.editor && current() == snapshot.editor && read(snapshot.editor, text) && text == snapshot.fullText;
}

bool EditorBridge::navigate(const Snapshot& snapshot, std::size_t begin, std::size_t end) const
{
    if (!unchanged(snapshot)) return false;
    const auto start = snapshot.start + static_cast<std::intptr_t>(begin);
    const auto finish = snapshot.start + static_cast<std::intptr_t>(end);
    send(snapshot.editor, Sci::SCI_SETSEL, start, finish);
    const auto line = send(snapshot.editor, Sci::SCI_LINEFROMPOSITION, start);
    send(snapshot.editor, Sci::SCI_ENSUREVISIBLE, line);
    send(snapshot.editor, Sci::SCI_SCROLLCARET);
    return true;
}

bool EditorBridge::replace(const Snapshot& snapshot, const QByteArray& replacement, QString& error) const
{
    if (!unchanged(snapshot))
    {
        error = QStringLiteral("The document changed. Refresh and try again.");
        return false;
    }
    if (send(snapshot.editor, Sci::SCI_GETREADONLY))
    {
        error = QStringLiteral("The document is read-only.");
        return false;
    }
    send(snapshot.editor, Sci::SCI_BEGINUNDOACTION);
    send(snapshot.editor, Sci::SCI_SETTARGETSTART, snapshot.start);
    send(snapshot.editor, Sci::SCI_SETTARGETEND, snapshot.end);
    send(snapshot.editor, Sci::SCI_REPLACETARGET, replacement.size(), reinterpret_cast<std::intptr_t>(replacement.constData()));
    send(snapshot.editor, Sci::SCI_ENDUNDOACTION);
    send(snapshot.editor, Sci::SCI_SETSEL, snapshot.start, snapshot.start + replacement.size());
    return true;
}
}
