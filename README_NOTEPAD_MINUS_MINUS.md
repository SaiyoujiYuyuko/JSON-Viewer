# JSON Viewer for Notepad--

本项目 fork 自 [NPP-JSONViewer/JSON-Viewer](https://github.com/NPP-JSONViewer/JSON-Viewer)，针对 Notepad-- 适配。插件通过 CMake 构建，复用上游解析依赖和格式化实现。

已在 **Notepad-- 3.9.0 / Windows x64 / Qt 5.15.2** 中加载并实测。插件名为 **JSON Viewer (Upstream)**，DLL 为 `ndd-json-viewer-upstream.dll`。这是本 fork 的适配版本，并非上游官方发布的 Notepad-- 插件。

## 安装与使用

1. 保存正在编辑的文件并退出 Notepad--。
2. 把编译出的 `ndd-json-viewer-upstream.dll` 复制到 Notepad-- 的 `plugin` 目录。保留宿主自带的 Qt 和 `qmyedit_qt5.dll`。
3. 启动 Notepad--，打开 JSON 文件，选择 **插件 → JSON Viewer (Upstream) → Show JSON Viewer**，或按 **Ctrl+Alt+Shift+J**。

安装目录示例：

```text
D:\Apps\Notepad--\plugin\ndd-json-viewer-upstream.dll
```

旧的 JSONview plug 可继续保留。两个插件各有菜单；旧面板可直接关闭。

| 操作 | 行为 |
|---|---|
| Show JSON Viewer | 展示当前整个文档，默认跟随切换的标签页 |
| View selected JSON | 展示选中的一个 JSON 片段；无选区时展示整个文档 |
| 树节点 `{n}` / `[n]` | 分别表示直接对象成员数量、直接数组元素数量 |
| 单击 / 双击节点 | 定位原文 / 选中原文键名；数组元素选中其值 |
| 右键 | 复制名称、原始 JSON 值、路径，展开或折叠子树 |
| 搜索框 + Enter | 搜索键或值，继续 Enter 查找下一项 |
| 工具栏 | 刷新、校验、格式化、全部展开/折叠、跟随文档 |
| 底部滑块 | 缩放树字体 |
| Format / Compress JSON | 操作选区或整个文档，可用一次 Ctrl+Z 撤销 |
| Sort keys (changes document) | 显式重排文档中的对象字段，数组顺序保持不变 |

树本身始终按原文顺序，不调用按字母排序。格式化与压缩也保留字段顺序和数字原始精度。只有显式的 Sort keys 操作改变字段顺序；含重复键或键名中含 `\u0000` 的文档拒绝执行此操作，仍可正常查看与格式化。

设置文件位于 `plugin/config/JSONViewer.Upstream.ini`，记录跟随文档、字体缩放、允许注释和尾逗号等选项。

回退：退出 Notepad-- 后移走新增的 `ndd-json-viewer-upstream.dll` 即可。原插件未被覆盖。

## 为什么旧移植版显示不同

[社区旧版](https://gitee.com/ndd-community/ndd-json-viewer/) 重写了宿主接口、解析和 Qt 界面。对象树遍历使用 `QJsonObject::keys()`；Qt 返回按键名排序的列表，原始字段顺序因此丢失。数组内部的对象字段变化也容易看起来像“数组乱序”。本适配版使用有序节点列表直接保存原文顺序。

移植关系不表示两仓库会自动同步。当前适配层复用本仓库的解析依赖和格式化实现，使后续合并上游改进更容易。

## 实现边界

- `src/NppJsonViewer/JsonHandler.*`：直接复用上游校验、格式化、压缩和排序实现。`ParseOptions` 抽到独立头文件，去除共享代码对原宿主设置的依赖。
- `src/JsonCore/OrderedJson.*`：使用仓库固定版本的 RapidJSON SAX 解析器，以有序子节点列表保存成员，保留空键、重复键、数字原文及 UTF-8 字节位置。不会经过 `QJsonObject` 或浮点数转换。
- `src/NddJsonViewer/`：Notepad-- 插件 ABI、编辑器桥接与 Qt dock 界面。每个宿主窗口独立持有回调。树选择定位、复制值和路径均来自同一份文档快照。
- 编辑器桥接通过已验证的非虚函数 `SendScintillaPtrResult` 获取 Scintilla direct 接口，以指针宽度传递消息；避免 Windows 上 `long` 为 32 位造成的截断。

当前版本使用 Qt 侧边栏界面。尚未移植 `undefined` 自动替换、打开文件自动格式化等功能。界面文字目前为英文。

输入限制为编辑器内部 UTF-8、最多 64 MiB、嵌套最多 512 层；所有树节点一次性构建，接近上限的大文件可能明显卡顿。注释和尾逗号默认允许；校验还沿用上游的 NaN/Infinity 等扩展，不等于严格 RFC JSON 校验。格式化/压缩会移除注释。选择模式下修改原文后需要重新选区并刷新。重复键的文本路径可能相同，但节点和原文定位仍各自保留。

## 从源码构建

需要 Git、Python 3、CMake 3.20+、Ninja、MSVC x64 工具链与 Windows SDK，以及 **Qt 5.15.2 msvc2019_64**。本次实际编译使用 MSVC 14.29.30133 和 Windows SDK 10.0.19041.0。

在已配置 MSVC x64 环境的终端中运行（路径按实际安装修改）：

```powershell
git submodule update --init external/rapidjson external/googletest
python tools/prepare_ndd_sdk.py --host 'D:/Apps/Notepad--'
cmake -S . -B .build/ndd -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH='C:/Qt/5.15.2/msvc2019_64' -DNDD_SDK_ROOT="$PWD/.build-tools/ndd-sdk"
cmake --build .build/ndd --parallel 4
ctest --test-dir .build/ndd --output-on-failure
```

输出：`.build/ndd/ndd-json-viewer-upstream.dll`。

`prepare_ndd_sdk.py` 从 Gitee 固定提交 `91105f68b74382128f3313ac5af8accdc77de918` 下载五个 QScintilla 头文件并验证 SHA-256，再从用户实际宿主 DLL 的导出表生成导入库。它不复制或替换宿主 DLL，来源与宿主哈希写入 SDK 的 `provenance.json`。

**ABI 注意**：公开头文件与 3.9.0 成品 DLL 并非每个虚函数签名都一致。当前插件只使用实测可用的非虚消息接口和 `textChanged` 信号，不构造或继承宿主编辑器。更换 Notepad-- 版本、Qt 版本、编译器 ABI 或调用额外 QScintilla 接口时，需要重新核对并在宿主内实测；重新生成导入库本身不保证兼容。

仅构建共享核心与测试，不需要 Qt 或 Notepad-- SDK：

```powershell
cmake -S . -B .build/core -DBUILD_NDD_PLUGIN=OFF
cmake --build .build/core --config Release
ctest --test-dir .build/core -C Release --output-on-failure
```

`.github/workflows/ndd-core.yml` 验证共享核心；宿主 GUI 需在目标安装环境中实测。

## 验证记录

- 46 个原有上游测试和 9 个新增测试：覆盖字段顺序、12+ 元素数组、数量、中文与转义字节位置、空/重复键、精度、所有根类型、错误位置、嵌套限制、排序保护。
- Notepad-- 3.9.0 实测加载插件；`in_Daily1.json` 的顶层顺序为 `type → startTime → forcastType → resData → wainData → wagaData → stData → regData`，数量包括 `wainData [23]`、`wagaData [2]`。
- `tests/fixtures/ndd-manual.json` 实测标签页跟随、数组 `[0]…[11]`、嵌套对象 `z → a`、中文显示与其后字段定位、精度保持、格式化和一次撤销。
- 自动测试覆盖共享核心；宿主交互为实际运行验证，不是自动化宿主集成测试。

## 后续合并上游

保留原上游目录结构；适配代码尽量集中在新增目录。同步上游时正常合并 Git 提交及对应子模块指针，重新运行共享核心测试，编译 DLL，并在目标宿主中检查加载、顺序、跳转、格式化与撤销。

上游 `JsonHandler` 和 RapidJSON 的修复可以通过这种方式继续受益；上游 Win32 UI 的新增功能仍需手动映射到 Qt 界面，不会自动出现。

依赖来源和许可见 [THIRD_PARTY_NOTICES_NDD.md](THIRD_PARTY_NOTICES_NDD.md)。
