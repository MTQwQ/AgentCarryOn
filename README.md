# AgentCarryOn

AgentCarryOn is a human-in-the-loop companion tool for long-running agent workflows.

It is designed for situations where an agent session is close to quota, close to interruption, or needs one more round of human guidance before the task is truly done. The tool lets the agent show a draft report or checkpoint file in a popup window, receive human instructions, and save the reviewed text for durable recordkeeping.

## Purpose

Many agent tools try to keep pushing until the current task is finished, even when usage is tight. AgentCarryOn turns that behavior into a practical workflow:

- The agent writes a draft summary, checkpoint, or report into a Markdown or text file.
- The agent launches AgentCarryOn with that file path.
- A human reads the file content in a scrollable window, types corrections or follow-up instructions, and can save the displayed text into a local folder.
- The agent continues from human feedback instead of stopping cold.

This is useful for:

- continuing work under quota pressure,
- collecting human corrections before the final response,
- preserving intermediate agent output as files,
- keeping a durable trail of what the agent was about to report.

## Current Status

The current repository reflects the latest project structure:

- Windows source: native C++ Win32 implementation
- Linux source: native C++ GTK4 implementation
- Linux release artifact: AppImage build flow with bundled CJK font support
- Source repository: no executables or build artifacts committed
- Release artifacts: expected to be uploaded separately on GitHub Releases

## Core Behavior

- A popup window asks for human input.
- While no input is submitted, the terminal prints a fake progress bar every 5 seconds.
- If a text file path is passed in, the file content is shown in a scrollable text area.
- A save button copies the displayed text into `./aimonitor_save` under the current working directory.
- After confirmation, the human input is printed back to the terminal and the app exits.

## Repository Layout

- `windows/aimonitor.cpp`
  Windows implementation source
- `linux/aimonitor.cpp`
  Linux GTK4 implementation source
- `linux/CMakeLists.txt`
  Linux build configuration
- `linux/build-appimage.sh`
  Linux AppImage packaging script
- `linux/AppDir/`
  Minimal AppDir template used during packaging
- `PROMPT.en.txt`
  Reusable English prompt text
- `PROMPT.zh-CN.txt`
  Reusable Chinese prompt text

## Build Notes

### Windows

This repository keeps the Windows source code only. The compiled `.exe` should be published through Releases, not committed into Git source history.

### Linux

The main Linux implementation is the GTK4 C++ application in `linux/aimonitor.cpp`.

The repository may still contain the older shell prototype `linux/aimonitor.sh`, but the current primary Linux release path is the C++ GTK4 build plus `build-appimage.sh`.

The AppImage packaging flow is designed to:

- bundle the Linux executable,
- collect runtime dependencies,
- include CJK font resources,
- improve Chinese text rendering on machines without preinstalled Chinese fonts.

## Prompt Rule

You can use the following rule or prompt in English:

```text
When you believe a task is close to completion, do not report to me directly. Instead, write the report text into a Markdown document, call aimonitor with the Markdown file path as an argument, and use the tool to receive the human's requested revisions. Even when no direct report is needed, you must still call aimonitor before the task is considered complete.
```

Chinese version:

```text
当你认为每个任务将要完成时，不要直接向我汇报，而是把准备汇报的文本写进一个 Markdown 文档，再调用 aimonitor，并把该 Markdown 文件路径作为参数传入。然后根据工具里收到的人类修改意见继续调整内容。即使当前不需要直接汇报，也必须在任务完成前调用一次 aimonitor。
```

## Release Guidance

The following files are intended for GitHub Releases rather than source control:

- Windows executable, English UI
- Windows executable, Chinese variant
- Linux `AgentCarryOn-x86_64.AppImage`

The source repository should remain clean and contain source code, scripts, templates, and documentation only.
