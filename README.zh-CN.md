# AgentCarryOn

AgentCarryOn 是一个给长时间运行的 agent 工作流配套使用的小工具。

它的目的很直接：当某些 agent 工具快没额度、快要中断，或者人想在最后阶段接管一下时，可以用 AgentCarryOn 让 agent 在“当前任务尽量做完”的惯性下继续干活，同时把阶段性文本和人工修改意见稳定地保存下来。

## 这个工具是干什么的

很多 agent 工具在额度紧张时，仍然会倾向于把当前任务继续往前推。AgentCarryOn 把这种行为变成一个更可控的工作流：

- agent 先把当前总结、汇报草稿或待确认内容写进一个 Markdown 文件；
- agent 启动 AgentCarryOn，并把这个文件路径作为参数传进去；
- 人类在带滚动条的窗口里查看文本，输入修改意见或新的指令；
- 同时可以把当前显示文本保存到本地目录，形成持久化留档；
- agent 再根据人类反馈继续工作，而不是直接停掉。

这样比较适合：

- 在额度快耗尽时继续推进任务；
- 把 agent 的阶段性回答落成文件；
- 收集人工修改意见；
- 保留 agent 原本准备汇报的内容，方便回看和复用。

## 核心行为

- 启动后弹出输入窗口，等待人类输入。
- 只要还没有确认输入，终端里就会每 5 秒刷新一次假的进度条。
- 如果传入了文本文件路径，就会在窗口里显示文本内容，并带滚动条。
- 点击保存按钮后，会把当前显示的文本复制到当前工作目录下的 `aimonitor_save` 文件夹。
- 点击确认后，输入内容会回显到终端，然后程序退出。

## Windows 版本

文件：

- `windows/aimonitor.cpp`
- `windows/aimonitor.exe`

运行方式：

```powershell
.\windows\aimonitor.exe
.\windows\aimonitor.exe C:\path\to\notes.md
```

## Linux 版本

文件：

- `linux/aimonitor.sh`
- `linux/AppDir/`
- `linux/build-appimage.sh`

Linux 版本当前使用 `zenity` 提供弹窗界面和文本查看窗口。

运行方式：

```bash
chmod +x ./linux/aimonitor.sh
./linux/aimonitor.sh
./linux/aimonitor.sh /path/to/notes.md
```

## 可直接复用的提示词 / 规则

你可以把下面这段当作规则或提示词使用：

```text
当你认为每个任务将要完成时，不要直接向我汇报，而是把准备汇报的文本写进一个 Markdown 文档，再调用 aimonitor，并把该 Markdown 文件路径作为参数传入。然后根据工具里收到的人类修改意见继续调整内容。即使当前不需要直接汇报，也必须在任务完成前调用一次 aimonitor。
```

英文版：

```text
When you believe a task is close to completion, do not report to me directly. Instead, write the report text into a Markdown document, call aimonitor with the Markdown file path as an argument, and use the tool to receive the human's requested revisions. Even when no direct report is needed, you must still call aimonitor before the task is considered complete.
```

## AppImage 说明

项目里已经带了 AppDir 目录结构和 AppImage 打包脚本，适合在 WSL 或 Linux 环境里继续出发布包。

如果 Linux 环境里已经有 `appimagetool`，可以执行：

```bash
cd linux
chmod +x build-appimage.sh aimonitor.sh AppDir/AppRun
./build-appimage.sh
```

生成的 AppImage 会放在 `linux/` 目录里。
