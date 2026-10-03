# 可选：替换经典 notepad.exe 的启动行为

[返回主说明](../README.md)

默认文件关联设置完成后，双击关联文件已经会打开 Notepad--。如果还需要让命令行 `notepad file.txt` 或其他程序启动经典 `notepad.exe` 时也打开 Notepad--，再使用本功能。

## 安装与恢复

| 入口 | 用途 | 权限 |
| --- | --- | --- |
| `optional/01-Replace-Notepad.cmd` | 备份现有配置并安装重定向 | 以管理员身份运行 |
| `optional/02-Restore-Notepad.cmd` | 恢复安装前的重定向配置 | 以管理员身份运行 |

它影响整台电脑的所有用户，读取与主说明相同的本地配置。安装时把 `tools/NotepadRedirect.exe` 复制到 Notepad-- 所在目录，再写入以下键；这里使用默认示例路径：

```text
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe
Debugger = "C:\Tools\Notepad--\NotepadRedirect.exe" --ifeo
```

转接程序去掉 Windows 追加的原始 `notepad.exe` 参数，再调用同目录的 Notepad--，支持中文、空格和特殊字符文件名。它不修改系统 `notepad.exe` 文件，也不经过命令解释器。安装目录应可供需要使用的用户访问。

备份在本机保存：

```text
%ProgramData%\NotepadMinusMinusPortable\Replacement\backup.json
%ProgramData%\NotepadMinusMinusPortable\Replacement\before.reg
```

原键不存在时不会生成 `before.reg`。恢复只处理原 `Debugger` 值，不删除整个 IFEO 键；若之后有其他程序改过该值，脚本停止以保留后续修改。不要把其他电脑的备份复制过来恢复。移动或删除 Notepad-- 前应先恢复重定向。

## 自定义路径和检查

在包根目录的管理员 PowerShell 中运行：

```powershell
.\tools\NotepadReplacement.ps1 -Action Install -EditorPath 'D:\Apps\Notepad--\Notepad--.exe'
.\tools\NotepadReplacement.ps1 -Action Restore
```

只读查看现状不需要管理员权限：

```powershell
.\tools\NotepadReplacement.ps1 -Action Status
```

已有本包重定向时，更换安装路径前先恢复，再重新安装。安装后在自己的命令提示符中测试 `notepad` 和 `notepad "C:\含空格的目录\测试.txt"`。

## 支持范围与源码

支持常见的空启动、打开单个文件和相对路径。旧 `/A`、`/W` 开关会被忽略，编码由 Notepad-- 检测；不模拟 `/P`、`/PT` 打印或其他专有开关，也不保证单实例编辑器的等待语义。Windows 11 商店版记事本的应用入口或执行别名可能不经过经典 EXE。

本功能与文件关联分开：恢复重定向不恢复文件关联，文件关联恢复也不撤销重定向。

完整源码为 `source/NotepadRedirect.cpp`。在 MSVC x64 Native Tools 命令行运行 `source/build.cmd` 可重建 `tools/NotepadRedirect.exe`。采用静态 C++ 运行库，运行时不需要 Qt 或 Python；Notepad-- 自身依赖仍由其目录提供。
