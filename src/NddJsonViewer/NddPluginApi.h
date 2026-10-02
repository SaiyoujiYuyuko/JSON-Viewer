#pragma once

#include <QString>
#include <QMenu>
#include <functional>

class QsciScintilla;

// Public Notepad-- 3.x plugin ABI. Field order and callback signatures must
// match src/pluginGl.h and src/plugin.h in cxasm/notepad--.
struct ndd_proc_data
{
    QString m_strPlugName;
    QString m_strFilePath;
    QString m_strComment;
    QString m_version;
    QString m_auther;
    int m_menuType = 0;
    QMenu* m_rootMenu = nullptr;
};
using NDD_PROC_DATA = ndd_proc_data;
using NddGetEditor = std::function<QsciScintilla*(QWidget*)>;
using NddHostCallback = std::function<bool(QWidget*, int, void*)>;
