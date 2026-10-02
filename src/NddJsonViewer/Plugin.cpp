#include "NddPluginApi.h"
#include "JsonViewer.h"
#include <QMainWindow>
#include <exception>

#ifdef _WIN32
#define JSON_VIEWER_EXPORT extern "C" __declspec(dllexport)
#else
#define JSON_VIEWER_EXPORT extern "C" __attribute__((visibility("default")))
#endif

JSON_VIEWER_EXPORT bool NDD_PROC_IDENTIFY(NDD_PROC_DATA* data)
{
    if (!data) return false;
    data->m_strPlugName = QStringLiteral("JSON Viewer (Upstream)");
    data->m_strComment = QStringLiteral("Source-order JSON tree with counts, navigation and current upstream formatting");
    data->m_version = QStringLiteral("1.0.0");
    data->m_auther = QStringLiteral("JSON-Viewer contributors; Notepad-- adapter");
    data->m_menuType = 1;
    return true;
}

JSON_VIEWER_EXPORT int NDD_PROC_MAIN(QWidget* host, const QString&,
    NddGetEditor getEditor, NddHostCallback, NDD_PROC_DATA* data)
{
    if (!host || !data || !data->m_rootMenu || !getEditor || !qobject_cast<QMainWindow*>(host)) return 1;
    // Each Notepad-- window receives its own callbacks and viewer instance.
    // Never put a host pointer or editor callback in a process-global variable.
    for (auto* child : host->children())
        if (child->objectName() == QStringLiteral("jsonViewerUpstreamPlugin")) return 0;
    try
    {
        new ndd::JsonViewer(host, std::move(getEditor), data->m_strFilePath, data->m_rootMenu);
        return 0;
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
