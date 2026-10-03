# 整理重复的右键菜单

[返回主说明](../README.md)

批量设置 `01-Set-Defaults.cmd` 成功后自动整理菜单；22 种类型已经关联时，重复运行也会执行整理并保留原关联备份。菜单整理只针对当前用户，普通权限运行。

| 入口 | 用途 |
| --- | --- |
| `optional/03-Clean-Context-Menu.cmd` | 单独整理菜单，不修改文件关联 |
| `optional/04-Restore-Context-Menu.cmd` | 恢复整理前的菜单可见性 |

整理保留本包的 **Edit with Notepad--**。已有的 `Notepad--` 和旧编辑器 `Notepad4` 菜单通过当前用户的 `LegacyDisable` 值隐藏；Windows 的“在记事本中编辑”通过当前用户的 Shell Extensions Blocked 设置隐藏。脚本从已安装的 Windows 记事本应用清单读取菜单扩展 CLSID，不依赖某个版本写死的标识。

这些设置只控制菜单显示，不卸载 Windows 记事本或 Notepad4，也不修改其他软件的右键功能。机器级的旧编辑器菜单通过当前用户的同名键隐藏，不修改机器级原键。Windows 记事本扩展同时提供文件和目录菜单时，两处都会隐藏。静态注册表文件本身只注册 **Edit with Notepad--**；直接导入 `.reg` 后，需另行运行菜单整理入口。

运行前会验证保留的菜单确实指向所配置的编辑器，随后备份原值到：

```text
%LOCALAPPDATA%\NotepadMinusMinusPortable\ContextMenu\backup.json
```

重复运行不丢失原值。恢复时只还原本工具改过的值；如果值已被其他软件改动，会停止并提示。备份含机器名和用户 SID，仅保留在原电脑，不提交到 Git。菜单整理与文件关联、`notepad.exe` 重定向分别恢复。

修改后重新打开右键菜单。若资源管理器仍缓存旧菜单，注销并重新登录后查看；脚本不会强行关闭资源管理器窗口。

只读检查可在工具目录的 PowerShell 中运行：

```powershell
.\tools\ContextMenu.ps1 -Action Status
```
