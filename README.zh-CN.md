# AgentCarryOn

英文 README： [README.md](./README.md)

AgentCarryOn 是一个给长时间运行的 agent 工作流配套使用的人机协同小工具。

它主要用于这些场景：agent 会话快到额度、快被中断，或者任务已经快结束但还需要人类再看一眼、改一轮。这个工具允许 agent 把阶段性文本结果以文件形式展示出来，让人类在弹窗里直接查看、修改、补充指令，并把文本保存下来做持久化留档。

## 这个项目的目的

很多 agent 工具在额度紧张时，仍然会尽量把当前任务继续往前推。AgentCarryOn 把这种行为变成一个可操作的工作流：

- agent 先把当前总结、汇报草稿或待确认内容写进一个 Markdown 或文本文件；
- agent 启动 AgentCarryOn，并把这个文件路径作为参数传进去；
- 人类在带滚动条的窗口里查看文本，输入修改意见或下一步指令，同时可以保存当前文本；
- agent 再根据人类反馈继续推进任务，而不是直接停掉。

它特别适合：

- 在额度快耗尽时继续完成当前任务；
- 在最终回复前插入一次人工校正；
- 把 agent 的阶段性输出落成文件；
- 保留 agent 原本准备汇报的内容，方便回看和复用。

## 当前状态

现在仓库里的结构和实现已经更新为：

- Windows：原生 C++ Win32 实现
- Linux：原生 C++ GTK4 实现
- Linux 发布物：使用 AppImage 打包，并额外带入 CJK 字体支持
- 源码仓库：不提交 exe、AppImage、build 目录等生成物
- 可执行文件：应单独放到 GitHub Releases 中发布

## 核心行为

- 启动后弹出输入窗口，等待人类输入。
- 只要还没有确认输入，终端里就会每 5 秒刷新一次假的进度条。
- 如果传入了文本文件路径，就会在窗口中显示文本内容，并带滚动条。
- 点击保存按钮后，会把当前显示的文本复制到当前工作目录下的 `aimonitor_save` 文件夹。
- 点击确认后，输入内容会回显到终端，然后程序退出。

## 仓库结构

- `windows/aimonitor.cpp`
  Windows 实现源码
- `linux/aimonitor.cpp`
  Linux GTK4 实现源码
- `linux/CMakeLists.txt`
  Linux 构建配置
- `linux/build-appimage.sh`
  Linux AppImage 打包脚本
- `linux/AppDir/`
  打包时使用的最小 AppDir 模板
- `PROMPT.en.txt`
  可直接复用的英文提示词
- `PROMPT.zh-CN.txt`
  可直接复用的中文提示词

## 构建说明

### Windows

仓库中只保留 Windows 源码，不把编译出的 `.exe` 提交进 Git 源码历史。Windows 可执行文件应放到 Releases 中分发。

### Linux

Linux 当前主实现是 `linux/aimonitor.cpp` 这份 GTK4 C++ 程序。

仓库里仍然可能保留早期的 shell 原型 `linux/aimonitor.sh`，但当前主要发布路线已经切换到 C++ GTK4 程序加 `build-appimage.sh`。

当前 AppImage 打包流程会尽量完成这些事情：

- 打入 Linux 可执行文件；
- 收集运行时依赖；
- 带入 CJK 字体资源；
- 尽量改善在没有预装中文字体的机器上显示中文的能力。

## 可直接复用的提示词 / 规则

中文：

```text
当你认为每个任务将要完成时，不要直接向我汇报，而是把准备汇报的文本写进一个 Markdown 文档，再调用 aimonitor，并把该 Markdown 文件路径作为参数传入。然后根据工具里收到的人类修改意见继续调整内容。即使当前不需要直接汇报，也必须在任务完成前调用一次 aimonitor。
```

英文：

```text
When you believe a task is close to completion, do not report to me directly. Instead, write the report text into a Markdown document, call aimonitor with the Markdown file path as an argument, and use the tool to receive the human's requested revisions. Even when no direct report is needed, you must still call aimonitor before the task is considered complete.
```

## Release 说明

下面这些文件应该作为 GitHub Release 资产上传，而不是提交进源码仓库：

- Windows 英文界面可执行文件
- Windows 中文版可执行文件
- Linux `AgentCarryOn-x86_64.AppImage`

源码仓库应尽量保持干净，只保留源码、脚本、模板和文档。
