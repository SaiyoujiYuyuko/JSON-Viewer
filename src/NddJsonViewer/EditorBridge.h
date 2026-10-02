#pragma once

#include "NddPluginApi.h"
#include <Qsci/qsciscintilla.h>
#include <QByteArray>
#include <QPointer>
#include <cstdint>

namespace ndd
{
struct Snapshot
{
    QPointer<QsciScintilla> editor;
    QByteArray fullText;
    QByteArray json;
    std::intptr_t start = 0;
    std::intptr_t end = 0;
    bool selection = false;
};

class EditorBridge
{
    QPointer<QWidget> host;
    NddGetEditor getEditor;
public:
    EditorBridge(QWidget* window, NddGetEditor callback);
    QsciScintilla* current() const;
    static std::intptr_t send(QsciScintilla* editor, unsigned message,
                              std::uintptr_t wParam = 0, std::intptr_t lParam = 0);
    bool capture(Snapshot& snapshot, bool selected, QString& error) const;
    bool unchanged(const Snapshot& snapshot) const;
    bool navigate(const Snapshot& snapshot, std::size_t begin, std::size_t end) const;
    bool replace(const Snapshot& snapshot, const QByteArray& replacement, QString& error) const;
};
}
