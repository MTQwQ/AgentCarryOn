# AgentCarryOn

AgentCarryOn is a companion utility for long-running agent workflows.

Its purpose is simple: when an agent session is close to its quota or about to stop, a human can use AgentCarryOn to keep the work moving. The tool lets the agent surface a draft report or checkpoint as text, receive human corrections through a popup input box, and preserve the reviewed content as a file for later reuse.

## Why this exists

Some agent tools tend to keep pushing until the current task is finished, even when the quota is tight. AgentCarryOn turns that behavior into a practical workflow:

- The agent writes the current summary or pending report into a Markdown file.
- The agent launches AgentCarryOn and passes the file path as an argument.
- The human reads the text in a scrollable window, types corrections or follow-up instructions, and can save the displayed text into a persistent folder.
- The agent continues working from the human feedback instead of stopping cold.

This makes it easier to:

- continue work under quota pressure,
- preserve intermediate answers as files,
- collect human corrections in a structured way,
- keep a durable audit trail of what the agent was about to report.

## Core behavior

- A popup window asks for human input.
- While no input is submitted, the terminal keeps printing a fake progress bar every 5 seconds.
- If a text file path is passed in, the file content is shown in a scrollable text area.
- A save button copies the displayed text into `./aimonitor_save` under the current working directory.
- After the human confirms, the input is printed back to the terminal and the app exits.

## Windows

Files:

- `windows/aimonitor.cpp`
- `windows/aimonitor.exe`

Run:

```powershell
.\windows\aimonitor.exe
.\windows\aimonitor.exe C:\path\to\notes.md
```

## Linux

Files:

- `linux/aimonitor.sh`
- `linux/AppDir/`
- `linux/build-appimage.sh`

The Linux version currently uses `zenity` for the popup UI and the text viewer.

Run:

```bash
chmod +x ./linux/aimonitor.sh
./linux/aimonitor.sh
./linux/aimonitor.sh /path/to/notes.md
```

## Release prompt text

You can use the following rule or prompt in English:

```text
When you believe a task is close to completion, do not report to me directly. Instead, write the report text into a Markdown document, call aimonitor with the Markdown file path as an argument, and use the tool to receive the human's requested revisions. Even when no direct report is needed, you must still call aimonitor before the task is considered complete.
```

## Notes on AppImage

This project includes an AppDir layout and a build script for AppImage packaging under WSL/Linux.

The AppImage build requires `appimagetool`. If it is available in the Linux environment, run:

```bash
cd linux
chmod +x build-appimage.sh aimonitor.sh AppDir/AppRun
./build-appimage.sh
```

The output file will be placed in the `linux/` directory.
