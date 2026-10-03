# JSON Viewer for Notepad--

[![Notepad-- shared core](https://github.com/SaiyoujiYuyuko/JSON-Viewer/actions/workflows/ndd-core.yml/badge.svg)](https://github.com/SaiyoujiYuyuko/JSON-Viewer/actions/workflows/ndd-core.yml)

本项目 fork 自 [NPP-JSONViewer/JSON-Viewer](https://github.com/NPP-JSONViewer/JSON-Viewer)，针对 **Notepad--** 进行适配，提供按 JSON 原文顺序展示、带各层元素数量的树形查看器。

适配层复用上游的解析依赖和格式化实现，并使用 Qt 构建侧边栏界面。后续可通过合并上游代码继续同步核心修复。

## 功能

- **保持原文顺序**：对象字段按出现顺序显示，数组元素按实际下标排列，包括 `[10]`、`[11]` 等。
- **显示各层数量**：`{n}` 表示对象的直接字段数，`[n]` 表示数组的直接元素数。
- **原文定位**：单击节点定位，双击选中键名或数组元素的值。
- **搜索与复制**：搜索键或值，复制名称、原始 JSON 值和节点路径。
- **树形操作**：展开、折叠、字体缩放，以及跟随当前文档。
- **格式化与压缩**：保留字段顺序和数字原始精度，支持一次撤销。
- **查看选区**：单独解析选中的 JSON 片段。

例如，文件中的 `{"z":1,"a":2}` 会按 `z → a` 展示。查看和格式化不会自动按字母重排字段；只有主动选择 **Sort keys (changes document)** 才会修改字段顺序。

## 兼容环境

已验证：**Notepad-- 3.9.0 / Windows x64 / Qt 5.15.2**。其他宿主版本需要重新核对接口并测试。界面文字目前为英文。

插件菜单名称：`JSON Viewer (Upstream)`。

插件文件名称：`ndd-json-viewer-upstream.dll`。

## 安装与使用

1. 按[构建说明](README_NOTEPAD_MINUS_MINUS.md#从源码构建)生成插件 DLL。
2. 保存文件并退出 Notepad--，将 DLL 复制到安装目录下的 `plugin` 文件夹。
3. 启动 Notepad-- 并打开 JSON 文件。
4. 选择 **插件 → JSON Viewer (Upstream) → Show JSON Viewer**，或按 **Ctrl+Alt+Shift+J**。

查看选区使用 **View selected JSON**。在树的搜索框输入内容后按 Enter 查找，继续按 Enter 查找下一项。

已有的 JSONview plug 可以与本插件并存。升级时退出宿主并替换本插件 DLL；卸载时退出宿主后移走该 DLL。

## Windows 默认文本编辑器设置

仓库还提供 [Notepad-- Windows 集成工具](Notepad--WindowsIntegration/README.md)，可批量关联 22 种文本文件，并检查或恢复原关联；另有可选的经典 `notepad.exe` 替换功能。

工具位于 `Notepad--WindowsIntegration/`，与查看器插件分别使用。源码包含构建和使用说明；个人安装路径通过不提交到 Git 的 `settings.local.json` 配置，机器备份与编译产物不入库。

## 构建与测试

构建依赖：MSVC x64、Windows SDK、Qt 5.15.2 msvc2019_64、CMake、Ninja 和 Python 3。仓库提供 `tools/prepare_ndd_sdk.py`，使用固定版本头文件和本机宿主 DLL 生成适配 SDK。

完整步骤、接口兼容说明和维护方法见 [Notepad-- 使用与构建说明](README_NOTEPAD_MINUS_MINUS.md)。

共享核心包含 **55 个自动测试**，覆盖顺序、精度、中文与转义位置、重复键和异常输入等；CI 使用 `.github/workflows/ndd-core.yml` 验证共享核心。宿主内已验证插件加载、标签页跟随、节点定位、格式化及撤销。

## 当前限制

- 编辑器内部需使用 UTF-8；支持最多 64 MiB 文本、512 层嵌套。树节点一次性构建，大文件可能卡顿。
- 注释和尾逗号默认允许，解析沿用上游部分 JSON 扩展。格式化或压缩会移除注释。
- 查看与格式化保留重复字段；显式排序拒绝重复键和含 `\u0000` 的键名。
- 上游新增的界面功能需要另行适配；当前尚未移植 `undefined` 自动替换、打开文件自动格式化等功能。

## 来源与许可

感谢[上游项目及贡献者](https://github.com/NPP-JSONViewer/JSON-Viewer/graphs/contributors)。本仓库代码沿用 [MIT 许可](LICENSE)，Qt、QScintilla 等依赖适用各自许可，详见[第三方依赖说明](THIRD_PARTY_NOTICES_NDD.md)。
