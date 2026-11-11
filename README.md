# RetroOS Terminal

<div align="center">

![C11](https://img.shields.io/badge/Language-C11%20Standard-blue?style=for-the-badge&logo=c)
![Electron](https://img.shields.io/badge/Frontend-Electron%20v33%20%2B%20xterm.js-47848F?style=for-the-badge&logo=electron)
![Node](https://img.shields.io/badge/Runtime-Node.js%2018%2B-339933?style=for-the-badge&logo=nodedotjs)
![Platform](https://img.shields.io/badge/Platform-Windows%2011%20%7C%20Linux%20%7C%20macOS-blueviolet?style=for-the-badge&logo=windows)
![Architecture](https://img.shields.io/badge/Architecture-x86__64-orange?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-ec4899?style=for-the-badge)

**A Next-Generation Cyberpunk Desktop Terminal & Hybrid Unix/Windows Operating Environment**

*Dual OS Boot Selector · POSIX Unix Engine · Windows CMD Compatibility · Braille Fastfetch Analytics*
*Interactive CPU Process Scheduler · Seamless Virtual Filesystem Mapping · Frameless OLED GUI*

</div>

---

## Table of Contents

- [1. Executive Summary](#1-executive-summary)
- [2. Architectural Overview](#2-architectural-overview)
- [3. Dual OS Execution Modes](#3-dual-os-execution-modes)
- [4. Dual Filesystem Mapping (VFS)](#4-dual-filesystem-mapping-vfs)
- [5. Complete Command Reference](#5-complete-command-reference)
  - [5.1 Windows CMD & System Commands](#51-windows-cmd--system-commands)
  - [5.2 POSIX Unix & Pipeline Commands](#52-posix-unix--pipeline-commands)
  - [5.3 Core OS Academic & Scheduling Commands](#53-core-os-academic--scheduling-commands)
  - [5.4 Hybrid Workflows (Piping Windows to Linux)](#54-hybrid-workflows-piping-windows-to-linux)
- [6. Desktop GUI & Interaction Features](#6-desktop-gui--interaction-features)
- [7. Step-by-Step Installation Guide](#7-step-by-step-installation-guide)
  - [7.1 Windows 10 / 11 (Recommended)](#71-windows-10--11-recommended)
  - [7.2 Linux (Ubuntu / Debian / Arch / Fedora)](#72-linux-ubuntu--debian--arch--fedora)
  - [7.3 macOS](#73-macos)
  - [7.4 Terminal-Only Mode (CLI Only)](#74-terminal-only-mode-cli-only)
- [8. System Shortcuts & Hotkeys](#8-system-shortcuts--hotkeys)
- [9. Repository Structure](#9-repository-structure)
- [10. Troubleshooting & FAQ](#10-troubleshooting--faq)
- [11. Contributing & License](#11-contributing--license)

---

## 1. Executive Summary

RetroOS Terminal is a standalone, hardware-accelerated desktop terminal application designed for software engineers, systems programmers, and computer science students. 

Traditional terminals force a strict choice: Windows developers must toggle between Command Prompt, PowerShell, and WSL bash windows. RetroOS Terminal bridges this divide by hosting a custom C11 Unix shell inside a frameless Electron xterm.js interface that natively understands both Windows filesystem conventions (`C:\`, `E:\`, `dir`, `cls`, `ipconfig`, direct `.exe` launches) and standard POSIX Unix pipelines (`grep`, `cat`, `sched`, `monitor`, background jobs).

---

## 2. Architectural Overview

The application is structured into three discrete execution layers:

```
+---------------------------------------------------------------------------------------+
|                                    PRESENTATION LAYER                                 |
|         Electron v33 Runtime + xterm.js Canvas + Nerd Font Typography + WebGL         |
|         Frameless Custom Titlebar + Dynamic Font Zoom (12px - 28px) + OLED Black      |
+---------------------------------------------------------------------------------------+
                                           |
                                [IPC PTY Bridge: node-pty]
                                           |
                                           v
+---------------------------------------------------------------------------------------+
|                                     SHELL ENGINE                                      |
|                 retroshell (C11 POSIX Engine with Windows Interop Layer)              |
|                                                                                       |
|   - Interactive OS Boot Selector [Windows | Linux | Hybrid]                           |
|   - Native Windows Drive Switcher (C:, E:, D:)                                        |
|   - Path Translator: /mnt/c/Users/... <---> C:\Users\...                              |
|   - Built-in OS Process Schedulers (FCFS & Preemptive Round Robin)                    |
|   - Live /proc Metrics Parser & ASCII Gantt Chart Generator                           |
|   - Automatic Windows Executable Dispatcher (.exe, .cmd)                              |
+---------------------------------------------------------------------------------------+
                         |                                           |
                         v                                           v
+--------------------------------------------------+ +----------------------------------+
|               LINUX UNIX SUBSYSTEM               | |          WINDOWS HOST OS         |
|   ext4 rootfs (/), /proc, /sys, bash utilities   | |   NTFS Drives (C:\, E:\), Apps   |
|   gcc, make, python3, coreutils, pipes, signals  | |   Explorer, VS Code, Task Manager|
+--------------------------------------------------+ +----------------------------------+
```

---

## 3. Dual OS Execution Modes

When launching RetroOS Terminal, the interactive **Boot Selector** allows the operator to select their operational focus. The active mode is permanently displayed on the prompt badge (`[WIN]`, `[LINUX]`, or `[HYBRID]`):

```text
  +-----------------------------------------------------------------------------+
  |                      R E T R O   O S   B O O T   L O A D E R                |
  |                      Select Operating Execution Environment                 |
  +-----------------------------------------------------------------------------+
    [1] Windows Engine Mode    -> Defaults to C:\, Windows CMD toolset
    [2] Linux Unix Engine Mode -> Defaults to ~/home, pure POSIX shell & schedulers
    [3] Hybrid Mode (Default)  -> Simultaneous dual compatibility
  +-----------------------------------------------------------------------------+
```

### Dynamic In-Session Mode Switching

You do not need to restart the application to transition between environments. Execute the `os` command at any time:

```bash
# Switch execution focus to Windows host environment
os win

# Switch execution focus to Linux Unix environment
os linux

# Switch execution focus to Hybrid mode
os hybrid

# Query current active environment state
os
```

---

## 4. Dual Filesystem Mapping (VFS)

RetroOS provides an inspection command (`fs` or `drives`) that details how host Windows partitions correspond to the Linux Virtual Filesystem:

```text
  ================== DUAL OS FILESYSTEM MAPPING ==================

  [WINDOWS HOST PARTITIONS]
  Logical Drive   WSL Mount Point   Filesystem Type   Status
  -------------   ---------------   ---------------   ------
  C:\             /mnt/c            NTFS / 9p         Active (System OS)
  E:\             /mnt/e            NTFS / 9p         Active (Project Workspace)

  [LINUX UNIX VIRTUAL FILESYSTEMS (VFS)]
  Virtual Path    Target Storage    Purpose
  ------------    --------------    -------
  /               ext4 rootfs       Unix Root System
  ~ (/home/...)   ext4 user home    Unix Configurations & Shell Binaries
  /proc           procfs (memory)   Kernel Process Tree & Metrics
  /sys            sysfs (kernel)    Device Drivers & Subsystems
```

---

## 5. Complete Command Reference

### 5.1 Windows CMD & System Commands

RetroOS Terminal provides native alias implementations of standard Windows Command Prompt utilities:

| Command | Syntax Example | Description |
| :--- | :--- | :--- |
| **Drive Switch** | `C:` or `E:` or `D:` | Instantly switch active working directory to specified Windows drive |
| **List Directory**| `dir` or `dir /path` | Formatted file and directory listing with file sizes and timestamps |
| **Clear Display** | `cls` | Clears terminal canvas and purges scrollback buffer |
| **Display File**  | `type README.md` | Prints textual file contents directly to stdout |
| **Make Directory**| `md new_folder` | Creates target directory structure (supports nested paths) |
| **Remove Dir**    | `rd old_folder` | Removes empty directory from storage |
| **Delete File**   | `del temp.txt` | Deletes targeted file permanently |
| **Copy File**     | `copy src.txt dst.txt`| Copies files or directories |
| **Move / Rename** | `ren old.txt new.txt` | Renames or moves files |
| **Networking**    | `ipconfig` | Displays IPv4, IPv6, Subnet, and Gateway adapters |
| **System Info**   | `systeminfo` | Detailed Windows OS version and hardware specification table |
| **Process List**  | `tasklist` | Displays active Windows tasks, memory consumption, and PIDs |
| **Terminate App** | `taskkill /IM notepad.exe /F` | Forcefully terminates target process by image name or PID |
| **Launch Apps**   | `explorer .`, `code .`, `notepad` | Launches Windows GUI applications directly without appending `.exe` |

### 5.2 POSIX Unix & Pipeline Commands

All standard POSIX tools are supported natively:

```bash
# File inspection and pattern matching
cat package.json | grep version

# Finding files recursively across drives
find . -name "*.c" -type f

# Disk and memory usage
df -h
free -m

# Pipelines and redirection
cat /proc/cpuinfo | grep "model name" | uniq
ls -lah > file_list.txt
cat < file_list.txt
```

### 5.3 Core OS Academic & Scheduling Commands

RetroOS Terminal contains a fully realized CPU scheduling simulator and benchmarking suite:

| Command | Example | Description |
| :--- | :--- | :--- |
| `sched fcfs` | `sched fcfs 0:5 1:3 2:8` | Non-preemptive FCFS CPU scheduling with arrival times and ASCII Gantt chart |
| `sched real` | `sched real rr 0.5 "burn 2"` | Preemptive Round Robin scheduling executed on live child processes |
| `monitor` | `monitor 1` | Real-time interactive CPU, RAM, and process activity monitor parsed from `/proc` |
| `mode-switch`| `mode-switch` | Toggles simulated privilege level between Ring 3 (User) and Ring 0 (Kernel) |
| `jobs` | `jobs` | Lists active asynchronous background jobs spawned via `&` |

### 5.4 Hybrid Workflows (Piping Windows to Linux)

Combine Windows system binaries with Unix utilities in a single command pipeline:

```bash
# Filter active Windows tasks through Unix grep
tasklist | grep -i electron

# Extract active IPv4 addresses from Windows ipconfig
ipconfig | grep -i "IPv4"

# Count source lines across a Windows project repository
dir | grep -E "\.c|\.js" | wc -l
```

---

## 6. Desktop GUI & Interaction Features

- **Pitch Black OLED Canvas**: Native `#000000` canvas background optimized for modern high-contrast monitors.
- **Dynamic Font Zooming**: Font sizes dynamically scale between `12px` and `28px` with glyph auto-rescaling (`rescaleOverlappingGlyphs: true`) to ensure Braille halftone art never breaks or misaligns.
- **Titlebar Telemetry**: Displays active font size indicator badge (`[A-] 15px [A+]`) alongside Sky Cyan (`−`), Emerald Green (`[ ]`), and Rose Red (`✕`) window controls.
- **Powerline Prompt Styling**: Unicode Powerline glyphs visually segment user identity, operational mode, and formatted Windows paths.

---

## 7. Step-by-Step Installation Guide

### 7.1 Windows 10 / 11 (Recommended)

#### Step 1: Ensure WSL 2 and Ubuntu are Installed
Open **PowerShell as Administrator**:
```powershell
wsl --install
```
*If WSL is already present, verify Ubuntu status via `wsl -l -v`.*

#### Step 2: Clone Repository
```powershell
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
```

#### Step 3: Compile C Shell Core
```powershell
wsl -e bash -c "mkdir -p ~/retroshell && cp retroshell.c ~/retroshell/ && cd ~/retroshell && gcc -std=c11 -Wall -o retroshell retroshell.c"
```

#### Step 4: Install Dependencies & Run
```powershell
npm install
.\launch.bat
```
*(You can also start the application using `npm start`).*

---

### 7.2 Linux (Ubuntu / Debian / Arch / Fedora)

```bash
# Ubuntu / Debian prerequisites
sudo apt update && sudo apt install -y build-essential gcc make git nodejs npm

# Clone repository
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell

# Compile C binary
make

# Install dependencies and launch
npm install
npm start
```

---

### 7.3 macOS

```bash
# Install Command Line Tools & Node via Homebrew
xcode-select --install
brew install node

# Clone and compile
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
make

# Launch desktop GUI
npm install
npm start
```

---

### 7.4 Terminal-Only Mode (CLI Only)

For resource-constrained headless environments or pure SSH sessions, run the compiled binary directly without Electron:

```bash
./retroshell
```

---

## 8. System Shortcuts & Hotkeys

| Shortcut | Scope | Action |
| :--- | :--- | :--- |
| `Ctrl` + `Alt` + `T` | Global (Windows) | Instant summon/focus RetroOS Terminal from anywhere |
| `Ctrl` + `=` | Terminal Window | Zoom In font size (+1px) |
| `Ctrl` + `-` | Terminal Window | Zoom Out font size (-1px) |
| `Ctrl` + `0` | Terminal Window | Reset font size to default (15px) |
| `Ctrl` + Mouse Wheel | Terminal Window | Smooth interactive font scaling |
| `Ctrl` + `Shift` + `N` | Terminal Window | Spawn an isolated new terminal session window |
| `Ctrl` + `C` | Shell Execution | Send SIGINT interrupt to active foreground process |

---

## 9. Repository Structure

```
RetroOS_Shell/
├── retroshell.c       # Primary C11 shell engine (VFS parser, scheduler, command router)
├── Makefile           # POSIX compilation specifications
├── main.js            # Electron main process (PTY manager, window state, global hotkeys)
├── index.html         # User Interface (xterm.js renderer, custom titlebar, zoom engine)
├── launch.bat         # Safe Windows launcher script (clears execution barriers)
├── icon.png           # High-resolution application artwork (512x512 PNG)
├── icon.ico           # Multi-resolution Windows system icon file (ICO)
├── fonts/             # Nerd Font binaries (Regular and Bold Cascadia/RetroNerd)
├── screenshots/       # Showcase screenshots for documentation
└── package.json       # Node package manifest and lifecycle scripts
```

---

## 10. Troubleshooting & FAQ

**Q: Why does the terminal show `retroshell: cannot switch to drive`?**  
A: Ensure the targeted drive letter is valid and formatted in Windows. For example, verify drive `E:\` is mounted and visible in Windows Explorer.

**Q: How do I change my shell username?**  
A: Type `user <new_name>` at any time. The shell updates your environment profile permanently.

**Q: How do I switch color themes?**  
A: Execute `theme <name>`. Available themes include `retro`, `cyber`, `matrix`, and `dracula`.

**Q: The application closes immediately upon opening.**  
A: Ensure the C binary is compiled inside WSL by executing:
```powershell
wsl -e bash -c "ls -la ~/retroshell/retroshell"
```
If missing, execute Step 3 of the Windows installation guide.

---

## 11. Contributing & License

Contributions, bug reports, and pull requests are welcomed. Please follow conventional commit guidelines when submitting contributions.

Licensed under the **[MIT License](LICENSE)**.

---

<div align="center">

Engineered by [SaKin32](https://github.com/SaKin32)

*"The shell is not a tool. It is your will, made executable."*

</div>
