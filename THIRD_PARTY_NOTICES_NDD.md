# Notepad-- adapter dependency notices

The original JSON-Viewer source and the new adapter source are provided under the repository's [MIT license](LICENSE). This does not change the licenses of dependencies or imply that the combined plugin binary is MIT-only.

| Component | Source | License / notes |
|---|---|---|
| JSON-Viewer code and reused icons | This repository, based on NPP-JSONViewer/JSON-Viewer | MIT; retain `LICENSE` |
| RapidJSON fork | `external/rapidjson`, pinned Git submodule | See `external/rapidjson/license.txt` for MIT and bundled component notices |
| GoogleTest | `external/googletest`, pinned Git submodule | BSD-3-Clause; test-only dependency |
| Qt 5.15.2 Core, Gui, Widgets | https://download.qt.io/archive/qt/5.15/5.15.2/ | LGPLv3/GPL/commercial terms as applicable; plugin dynamically uses the host's Qt DLLs |
| Notepad-- QScintilla interface | https://gitee.com/cxasm/notepad--/tree/91105f68b74382128f3313ac5af8accdc77de918/src/qscint | The downloaded headers retain Riverbank's GPLv3/commercial notices. The plugin dynamically links the host's `qmyedit_qt5.dll` |

The SDK preparation script retains the downloaded header notices and records the exact revision and hashes. No Qt, QScintilla, Notepad-- executable, or Microsoft toolchain binaries are committed or bundled with this adapter.

For redistribution of a built plugin, comply with the applicable Qt and QScintilla license terms, including corresponding-source and notice requirements where applicable, or use appropriate commercial licenses. In particular, linking to QScintilla does not become unrestricted merely because this repository's own code is MIT. This local build is not a claim to relicense third-party code.

GPLv3 text: https://www.gnu.org/licenses/gpl-3.0.html

LGPLv3 text: https://www.gnu.org/licenses/lgpl-3.0.html
