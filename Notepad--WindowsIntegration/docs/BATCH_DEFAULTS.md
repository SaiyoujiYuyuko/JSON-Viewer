# 批量文件关联说明

[返回主说明](../README.md)

普通权限双击根目录的 `01-Set-Defaults.cmd`，即可注册 Notepad--、备份当前用户的原选择、设置全部 22 种文件类型并逐项验证，然后整理重复的 Notepad--、旧 Notepad4 和 Windows 记事本菜单。无需先导入 `manual/Register-Notepad--.reg`。请在需要设置的用户账户下运行，不要以管理员身份运行。

程序路径与扩展名列表见[主说明](../README.md)。CSV、SQL 等列出的类型也会改为使用 Notepad--。

## 设置原理与兼容范围

Windows 的 `UserChoice` 包含与用户、扩展名、程序和时间相关的校验值。普通静态注册表文件只能注册候选程序，批量工具会在目标电脑现场计算对应记录。

部分新版 Windows 11 增加了 `UserChoiceLatest`。本包结合两种公开源码实现：

- 旧 `UserChoice`：`source/sfta/SFTA.ps1` 中的 Set-FTA。
- 新 `UserChoiceLatest`：`tools/AssociationBridge.exe`，包含限定文本类型的写入、校验与还原逻辑。

新版模式在写入前用本机现有记录验证算法兼容性；不匹配就停止。设置时同步两套记录，并通过 Windows 的 `QueryCurrentDefault` 和 `AssocQueryString` 检查当前选择和启动路径。脚本不修改 UCPD 服务、系统策略或注册表 ACL。

这是依赖公开实现的适配工具，**微软没有保证该设置方法始终兼容，Windows 更新可能改变算法或拒绝写入**。已在 Windows 11 25H2 环境验证批量设置；这不代表所有 Windows 版本都已验证。旧版 Windows 没有 UserChoiceLatest 时使用旧引擎，尚未在旧版系统实测。

若新版系统没有用于验证算法的现有记录，脚本会提示先在 Windows 设置中把 `.txt` 设置一次再重试；不需要逐个设置全部类型。

## 判断结果

成功时最后显示 `22 associations verified.`。每项 `OK` 和汇总信息是本次运行的检查结果。若某项失败，脚本尝试还原该项并报告失败，其他成功项保留；请查看结果日志，不要把部分成功当成全部完成。也可双击一个 `.txt` 和 `.json`，确认由预期的 Notepad-- 打开。

- `02-Check-Defaults.cmd`：只读查看新旧记录与 Windows 有效选择。
- `03-Restore-Defaults.cmd`：按最近一次备份恢复该批文件类型。

备份和结果文件位于：

```text
%LOCALAPPDATA%\NotepadMinusMinusPortable\Associations\<本次运行目录>\before.json
%LOCALAPPDATA%\NotepadMinusMinusPortable\Associations\<本次运行目录>\apply-result.json
%LOCALAPPDATA%\NotepadMinusMinusPortable\Associations\<本次运行目录>\restore-result.json
```

恢复时重新计算必要的 hash，不能把旧 Hash 字符串直接导回注册表。备份仅适用于原电脑和原用户；若某项之后被改成第三个程序，恢复脚本保留该选择并报告，避免覆盖后续修改。

原来没有明确默认程序的类型，恢复为没有显式选择；Windows 仍可能从已注册的程序中选择后备项。恢复不删除候选程序注册和右键菜单，也不改变可选的 `notepad.exe` 重定向。如需移除注册，见[主说明](../README.md#手工方式与取消注册)。所有类型已正确设置时，重复运行不会覆盖上一份备份。

菜单可见性另有独立备份，使用 `optional/04-Restore-Context-Menu.cmd` 恢复，见[菜单整理说明](CONTEXT_MENU.md)。菜单整理失败时会单独报错，已经验证成功的文件关联仍然保留。

## 自定义路径或历史备份

在包根目录的普通 PowerShell 中运行：

```powershell
.\tools\BatchDefaults.ps1 -Action Apply -EditorPath 'D:\Apps\Notepad--\Notepad--.exe'
.\tools\BatchDefaults.ps1 -Action Restore -BackupPath "$env:LOCALAPPDATA\NotepadMinusMinusPortable\Associations\某次运行\before.json"
```

运行时使用包内源码和本地编译程序，不下载或远程执行脚本。PowerShell 执行策略若阻止运行，请按本机管理规则处理；本包不会自行修改策略。

## 源码与许可

- [PS-SFTA](https://github.com/DanysysTeam/PS-SFTA)：固定提交 `22a32292e576afc976a1167d92b50741ef523066`，MIT / Copyright 2022 Danysys。源码与许可在 `source/sfta/`；本地仅移除了调用“应用关联通知设置”的那一行，保留用户通知偏好。
- [UserChoiceLatestHash](https://github.com/cssxn/UserChoiceLatestHash)：固定提交 `662070e5cdea4496bc474f4bdfd81f77d4cabab3`，MIT。算法、表文件与完整许可在 `source/ucl/`。
- `source/AssociationBridge.cpp` 与 `tools/BatchDefaults.ps1` 是本地适配代码。在 MSVC x64 环境运行 `source/build-associations.cmd` 可重建 `tools/AssociationBridge.exe`。
- 本包不包含或依赖 SetUserFTA 二进制。
