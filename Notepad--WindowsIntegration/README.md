# Notepad-- Windows 集成工具

把便携版 Notepad-- 一次设为 22 种文本文件的默认程序。适用于 Windows 10/11 x64，使用时保留各子目录。

默认示例路径：

```text
C:\Tools\Notepad--\Notepad--.exe
```

## 首次从源码使用

仓库提交脚本和源码，辅助 EXE、生成的 `.reg` 文件和 ZIP 不入库。先安装 MSVC C++ 工具与 Windows SDK，在 **x64 Native Tools Command Prompt** 中进入本目录并运行：

```bat
source\build-associations.cmd
source\build.cmd
```

分别生成 `tools/AssociationBridge.exe`（批量关联必需）和 `tools/NotepadRedirect.exe`（可选的系统记事本替换）。运行工具本身不需要 Python、Qt 或编译器。

如果编辑器不在默认示例路径，在普通 PowerShell 中进入本目录，复制配置模板并填写自己的绝对路径：

```powershell
Copy-Item .\settings.example.json .\settings.local.json
```

用文本编辑器修改 `settings.local.json` 的 `EditorPath`，JSON 路径中的反斜杠需要写成 `\\`。三个入口会读取此本地配置；可选的记事本替换和注册表生成器也使用它。`settings.local.json` 已被 Git 忽略，避免提交个人安装路径。已有配置时直接编辑，不必重新复制模板。

路径优先级：显式 `-EditorPath` 参数 → `settings.local.json` → 默认示例路径。

## 日常只用这三个入口

| 文件 | 作用 |
| --- | --- |
| [01-Set-Defaults.cmd](01-Set-Defaults.cmd) | 一键注册、备份、批量设置并验证 22 种文件关联 |
| [02-Check-Defaults.cmd](02-Check-Defaults.cmd) | 只读检查当前关联 |
| [03-Restore-Defaults.cmd](03-Restore-Defaults.cmd) | 按最近一次备份恢复原关联；需要回退时才运行 |

**准备好辅助程序和路径后，设置只需双击 `01-Set-Defaults.cmd`，普通权限运行，不要选择“以管理员身份运行”。** 无需先导入注册表，也不用去 Windows 设置逐项选择。成功时显示 `22 associations verified.`。

处理的扩展名：

```text
.txt .log .ini .cfg .conf .config .json .jsonc .xml .yaml .yml
.toml .md .markdown .csv .tsv .properties .lst .nfo .sql .srt .ass
```

## 子目录用途

| 目录 | 内容 |
| --- | --- |
| `optional/` | 可选的系统 `notepad.exe` 替换与恢复入口，需要管理员权限 |
| `manual/` | 手工注册、取消注册与打开 Windows 默认应用设置；日常批量设置不需要使用 |
| `tools/` | 主入口调用的 PowerShell 脚本和辅助程序 |
| `docs/` | [批量关联说明](docs/BATCH_DEFAULTS.md)与[系统记事本替换说明](docs/NOTEPAD_REPLACEMENT.md) |
| `source/` | 辅助程序源码、构建脚本、校验脚本及第三方许可；批量设置还需要其中的 `sfta/` |

个人配置、生成的注册表文件、备份、运行日志、编译产物与 `.build/` 缓存都被 Git 忽略。请勿使用 `git add -f` 将这些本地材料加入提交。

如果还要让命令行或其他程序启动经典 `notepad.exe` 时打开 Notepad--，右键 `optional/01-Replace-Notepad.cmd`，以管理员身份运行。回退使用同目录的 `02-Restore-Notepad.cmd`，也需要管理员权限。详见替换说明。

## 在其他电脑使用

可复制已编译的完整工具目录，在目标电脑调整 `settings.local.json` 后双击 `01-Set-Defaults.cmd`；也可从源码按上面的步骤构建。默认关联按用户保存；每个需要设置的用户分别运行一次。对外分享时只携带配置模板，移除本地配置、生成的 `.reg`、缓存、日志和备份。

若编辑器路径不同，在普通 PowerShell 中进入本包目录后运行：

```powershell
.\tools\BatchDefaults.ps1 -Action Apply -EditorPath 'D:\Apps\Notepad--\Notepad--.exe'
```

检查与恢复仍使用根目录的 `02`、`03` 入口。临时传入 `-EditorPath` 不会更新本地配置；需要长期使用该路径时，修改 `settings.local.json`。

## 手工方式与取消注册

手工 `.reg` 文件需先运行 `tools/Generate-RegistryFiles.ps1` 生成。`manual/Register-Notepad--.reg` 只注册当前用户的候选程序和右键菜单，随后可用 `manual/Open-DefaultApps.cmd` 打开系统设置自行选择默认程序。普通静态 `.reg` 文件不能可靠覆盖 Windows 已有的默认选择。

要取消本包注册，先用 `03-Restore-Defaults.cmd` 恢复原关联并检查结果，或在 Windows 设置中选回其他程序，再导入 `manual/Unregister-Notepad--.reg`。取消注册不撤销可选的 `notepad.exe` 替换。

如需为其他路径生成手工注册文件：

```powershell
.\tools\Generate-RegistryFiles.ps1 -EditorPath 'D:\Apps\Notepad--\Notepad--.exe'
```

输出在 `manual/`，生成动作不会应用注册表设置。省略 `-EditorPath` 时读取本地配置或默认示例路径。生成文件包含实际路径，不提交到 Git。

## 备份与兼容性

文件关联备份保存在 `%LOCALAPPDATA%\NotepadMinusMinusPortable\Associations`；可选记事本替换的备份保存在 `%ProgramData%\NotepadMinusMinusPortable\Replacement`。移动工具目录不改变备份位置，恢复入口可继续读取此前的备份。备份只能用于原电脑和原用户，其中可能包含用户名路径、机器名和 SID，请保留在本机。

批量设置依赖 Windows 关联校验的公开实现，Windows 更新可能影响兼容性。脚本会先检查兼容性，再验证每项设置结果；有失败时请查看窗口和结果日志。详见[批量关联说明](docs/BATCH_DEFAULTS.md)。这是本地适配工具，不是 Notepad-- 或微软官方组件。

## 只读验证

在本目录的 PowerShell 中运行以下命令。Python 3 仅供验证包结构，不是日常运行依赖。

```powershell
.\tools\AssociationBridge.exe self-test
.\tools\Generate-RegistryFiles.ps1
$editor = & .\tools\Get-EditorPath.ps1
python .\source\verify_package.py . $editor
.\tools\BatchDefaults.ps1 -Action Status
```

这组命令只生成本地注册文件并检查状态，不导入注册表或更改文件关联。
