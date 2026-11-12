# RetroOS Terminal

<div align="center">

![C11](https://img.shields.io/badge/Language-C11%20Standard-blue?style=for-the-badge&logo=c)
![Electron](https://img.shields.io/badge/GUI-Electron%20v33%20%2B%20xterm.js-47848F?style=for-the-badge&logo=electron)
![Platform](https://img.shields.io/badge/Primary%20Platform-Windows%2011%20(WSL2)-0078D6?style=for-the-badge&logo=windows)
![CLI](https://img.shields.io/badge/CLI%20Platform-Linux%20%7C%20macOS-339933?style=for-the-badge&logo=linux)
![License](https://img.shields.io/badge/Academic%20Project-Personal%20Use-ec4899?style=for-the-badge)
![Version](https://img.shields.io/badge/Version-1.3.0-c084fc?style=for-the-badge)

**A Cyberpunk Desktop Terminal and Educational Unix/Windows Hybrid Shell**

*POSIX Unix Core · Dynamic OS Boot Selector · Windows Command Interop Bridge*  
*Bash-style Arithmetic Expansion · CPU Process Scheduler · Live System Monitor*

</div>

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Features at a Glance](#2-features-at-a-glance)
3. [Technical Architecture](#3-technical-architecture)
4. [OS Boot Selector](#4-os-boot-selector)
5. [Shell Features in Detail](#5-shell-features-in-detail)
6. [Command Reference](#6-command-reference)
7. [Keyboard Shortcuts](#7-keyboard-shortcuts)
8. [Installation Guide](#8-installation-guide)
9. [Project Structure](#9-project-structure)
10. [Academic Context](#10-academic-context)
11. [Project Status & Terms](#11-project-status--terms)

---

## 1. Project Overview

RetroOS Terminal is a fully custom, cyberpunk-styled interactive terminal environment built from scratch in **C11** (the shell engine) and **Electron + xterm.js** (the GUI layer). It runs as a standalone desktop application on **Windows 11 with WSL2** and as a native CLI shell on **Linux and macOS**.

The project was originally developed as an operating systems course assignment and has grown into a feature-complete shell demonstrating real OS concepts: process scheduling, job control, pipe pipelines, I/O redirection, arithmetic expansion, dynamic filesystem mapping, and live `/proc`-based system monitoring — all within a cohesive retro-futuristic aesthetic.

---

## 2. Features at a Glance

| Category | Features |
| :--- | :--- |
| **Shell Core** | fork/exec pipelines, I/O redirection (`>`, `>>`, `<`, `2>`, `2>&1`), background jobs (`&`), job control (`jobs`), command chaining (`;`, `&&`, `\|\|`) |
| **Variables** | `export NAME=value`, `unset NAME`, `$VAR`, `${VAR}`, `$?`, `$$` expansion |
| **Arithmetic** | Bash-style `$((expr))` with `+`, `-`, `*`, `/`, `%`, bitwise (`&`, `\|`, `^`), shifts (`<<`, `>>`), comparisons, nested parentheses |
| **OS Interop** | Windows drive switching (`C:`, `E:`), CMD aliases (`dir`, `cls`, `type`), Windows binary dispatch (`ipconfig`, `tasklist`, `systeminfo`, `explorer`) |
| **Boot Selector** | Interactive 3-mode boot loader: Windows Engine, Linux Unix, Hybrid Interop |
| **Scheduler** | Simulated FCFS with Gantt chart + real preemptive Round Robin on live child processes |
| **Monitor** | Live htop-style dashboard: CPU cores, RAM, swap, process list, states — all from `/proc` |
| **Fastfetch** | Braille halftone cat art + dynamic system info (Kernel, Host, IP, RAM, CPU cores, uptime) |
| **GUI** | Frameless Electron window, Cyber Purple theme, font zoom (`Ctrl+scroll`), copy/paste, right-click paste |
| **Clipboard** | `Ctrl+C` (copy selection), `Ctrl+V` (paste), `Ctrl+Shift+C/V`, right-click (copy or paste) |
| **History** | Per-session command history with `history` command |
| **Themes** | `cyber` (default), `retro`, `matrix`, `dracula` |

---

## 3. Technical Architecture

```
+-----------------------------------------------------------------------------------+
|                            ELECTRON GUI LAYER (index.html)                        |
|          xterm.js Canvas · Frameless Titlebar · Font Zoom (12-28px)               |
|          Ctrl+C/V Copy-Paste · Right-Click Paste · CRT Scanline Effect            |
+-----------------------------------------------------------------------------------+
                                         |
                                    [node-pty]
                              (PTY bridge via IPC)
                                         |
                                         v
+-----------------------------------------------------------------------------------+
|                         RETROSHELL ENGINE  (retroshell.c / C11)                   |
|                                                                                   |
|   Tokenizer + Variable Expander      Pipeline Builder + Executor                  |
|   Arithmetic Evaluator $((expr))     Job Control + Signal Handling                |
|   Built-in Commands Dispatcher       Glob Expansion + I/O Redirection             |
|   History Engine + Theme Renderer    Fastfetch + Monitor + Scheduler              |
+-----------------------------------------------------------------------------------+
                    |                                         |
                    v                                         v
+-------------------------------------+   +---------------------------------------+
|        LINUX / WSL2 ENVIRONMENT     |   |        WINDOWS HOST ENVIRONMENT       |
|  /proc, /sys, /mnt, fork(), exec()  |   |  /mnt/c, /mnt/e, System32 binaries   |
|  Native Linux tools: ls, gcc, make  |   |  ipconfig.exe, explorer.exe, etc.     |
+-------------------------------------+   +---------------------------------------+
```

### Clarification on Platform Support

| Mode | Supported On | What Works |
| :--- | :--- | :--- |
| **Full Desktop GUI** | Windows 10/11 with WSL2 | Electron window + full retroshell with Windows interop |
| **Native CLI** | Linux, macOS, WSL2 | `./retroshell` in any POSIX terminal, all features except Windows dispatch |
| **Windows commands** | Windows + WSL2 only | Dispatched via `/mnt/c/Windows/System32/*.exe` through WSL interop |

---

## 4. OS Boot Selector

Every time you launch RetroOS Terminal, an interactive boot loader appears:

```
  +-----------------------------------------------------------------------------+
  |              R E T R O   O S   B O O T   L O A D E R                        |
  +-----------------------------------------------------------------------------+

  [1] Windows Engine Mode
      -> Defaults to your Windows User Profile (C:\Users\<you>)
      -> Windows CMD commands: dir, cls, ipconfig, tasklist, explorer

  [2] Linux Unix Engine Mode
      -> Defaults to ~/home directory
      -> Pure POSIX shell, schedulers, Linux tools

  [3] Hybrid Mode  (Recommended)
      -> Seamless dual: run Windows apps + Linux OS in one shell

  Select Mode [1, 2, or 3] (Press Enter for Hybrid):
  +-----------------------------------------------------------------------------+
```

### Live Mode Switching (No Restart Required)

| Command | Effect |
| :--- | :--- |
| `os win` | Switch to Windows Engine Mode (home set to Windows User profile) |
| `os linux` | Switch to Linux Unix Mode (home set to `~`) |
| `os hybrid` | Switch to Hybrid Interop Mode |
| `os` | Print current active mode and working directory |

---

## 5. Shell Features in Detail

### Arithmetic Expansion

RetroOS supports bash-compatible `$((expression))`:

```bash
echo $((6 * 7))               # 42
echo $(( (10 + 5) * 2 ))      # 30
echo $(( 2 ** 0 ))            # Note: ** unsupported, gives clear error
echo $(( 4 | 1 ))             # 5  (bitwise OR)
echo $(( 1 << 4 ))            # 16 (left shift)
X=5; echo $(( X * 2 ))        # 10 (variable lookup)
echo $((-(-9223372036854775807-1)))  # Handles LLONG_MIN edge case safely
```

Supported operators: `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `~`, `!`, `<<`, `>>`, `==`, `!=`, `<`, `<=`, `>`, `>=`, nested `()`.  
Division by zero and syntax errors produce clean error messages without crashing.

### Pipelines and Redirection

```bash
ls -la | grep ".c" | sort
cat file.txt | wc -l
gcc main.c -o main 2>&1 | head -20
echo "log entry" >> logfile.txt
sort < unsorted.txt > sorted.txt
sleep 10 &                      # background job
jobs                            # list background jobs
```

### Variable Expansion and Export

```bash
export NAME=SaKin
echo "Hello, $NAME"
export PATH=$PATH:/custom/bin
export FOO=baz; echo $FOO       # Correct JIT expansion: prints "baz"
unset NAME
export                          # list all environment variables
```

### Filesystem Mapping

Run `fs` or `drives` to inspect the dual-OS filesystem layout:

```
  ================== DUAL OS FILESYSTEM MAPPING ==================

  [DETECTED WINDOWS HOST PARTITIONS]
  Drive Letter    WSL Mount Point   Filesystem Type   Status
  ------------    ---------------   ---------------   ------
  C:\             /mnt/c            DrvFs / 9p        Active (Accessible)
  E:\             /mnt/e            DrvFs / 9p        Active (Accessible)

  [LINUX UNIX VIRTUAL FILESYSTEMS (VFS)]
  Virtual Path    Target Storage    Purpose
  /               rootfs            Unix Root Environment
  ~ (/home/...)   ext4 user home    User Home Directory
  /proc           procfs (RAM)      Kernel Virtual Process Tree
  /sys            sysfs (RAM)       Device Drivers and Kernel Subsystems
```

---

## 6. Command Reference

### Navigation and Filesystem

| Command | Description |
| :--- | :--- |
| `cd [dir]` | Change directory (`cd -` for previous, `cd ~` for home) |
| `pwd` | Print working directory |
| `C:` / `D:` / `E:` | Switch directly to Windows drive root |
| `ls`, `cat`, `grep`, `find` | Standard Unix tools (passed to system) |
| `dir [path]` | List files (Windows CMD alias for `ls -lah`) |
| `type <file>` | Display file contents (alias for `cat`) |
| `md <folder>` | Create directory (alias for `mkdir -p`) |
| `rd <folder>` | Remove directory (alias for `rmdir`) |
| `del <file>` | Delete file (alias for `rm -f`) |
| `copy` / `move` / `ren` | Copy, move, rename (aliases for `cp -r`, `mv`) |

### Windows Interop Commands (WSL2 only)

| Command | Description |
| :--- | :--- |
| `ipconfig` | Display Windows network adapters |
| `tasklist` | Display Windows running process list |
| `systeminfo` | Detailed Windows host diagnostics |
| `explorer .` | Open Windows Explorer at current path |
| `notepad <file>` | Open file in Windows Notepad |
| `code .` | Open VS Code in current directory |

### Academic and OS Core Features

| Command | Example | Description |
| :--- | :--- | :--- |
| `sched fcfs` | `sched fcfs 0:5 1:3 2:8` | Simulated FCFS scheduler with ASCII Gantt chart, WT and TAT averages |
| `sched real` | `sched real rr 0.5 "burn 2" "burn 1"` | Real preemptive Round Robin: forks real child processes, measures actual CPU time, draws live Gantt |
| `monitor` | `monitor 1` | Live htop-style dashboard: per-core CPU usage, RAM/swap bars, process table sorted by CPU or RAM |
| `monitor once` | `monitor once` | Single-shot snapshot of system state |
| `mode-switch` | `mode-switch` | Simulated privilege level toggle: Ring 3 (User) to Ring 0 (Kernel) |

### Shell and UI Commands

| Command | Description |
| :--- | :--- |
| `fastfetch` / `fetch` | Display cyberpunk braille cat art and live system info |
| `theme <name>` | Switch terminal color theme: `cyber`, `retro`, `matrix`, `dracula` |
| `user <name>` | Set or view your shell username (persisted in `~/.retroshell_user`) |
| `welcome` | Re-display the first-launch welcome screen |
| `history` | Show current session command history |
| `jobs` | List active background job PIDs |
| `export [NAME=val]` | Set or list environment variables |
| `unset NAME` | Remove an environment variable |
| `os [win\|linux\|hybrid]` | Switch or query OS execution mode |
| `fs` / `drives` | Display dual-OS filesystem mapping |
| `clear` / `cls` | Clear screen and scrollback buffer |
| `exit [code]` | Exit the shell |

---

## 7. Keyboard Shortcuts

| Shortcut | Scope | Action |
| :--- | :--- | :--- |
| `Ctrl` + `C` *(with text selected)* | Terminal | Copy selected text to clipboard |
| `Ctrl` + `C` *(nothing selected)* | Terminal | Send SIGINT to running process (normal interrupt) |
| `Ctrl` + `V` | Terminal | Paste from clipboard |
| `Ctrl` + `Shift` + `C` | Terminal | Copy selected text (alternative) |
| `Ctrl` + `Shift` + `V` | Terminal | Paste from clipboard (alternative) |
| Right Click *(with selection)* | Terminal | Copy selected text |
| Right Click *(no selection)* | Terminal | Paste from clipboard |
| `Ctrl` + `=` / `+` | Terminal | Zoom In font size (+1px) |
| `Ctrl` + `-` | Terminal | Zoom Out font size (-1px) |
| `Ctrl` + `0` | Terminal | Reset font size to default (15px) |
| `Ctrl` + Mouse Wheel | Terminal | Smooth font scaling |
| `Ctrl` + `Shift` + `N` | Terminal | Open a new RetroOS Terminal window |

> **Global Hotkey (Windows):** Create a desktop shortcut to `launch.vbs` and assign any hotkey (e.g. `Ctrl+Alt+3`) in the shortcut Properties to open RetroOS from anywhere.

---

## 8. Installation Guide

### Windows 10 / 11 — Full Desktop GUI (Recommended)

#### Prerequisites
- Windows 10 version 2004 or Windows 11
- WSL2 with a Linux distribution (Ubuntu recommended)
- Node.js 18+ installed on Windows
- Git for Windows

#### Step 1: Install WSL2

Open PowerShell as Administrator:

```powershell
wsl --install
```

Restart your computer when prompted, then open Ubuntu and complete the initial setup.

#### Step 2: Clone the Repository

```powershell
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
```

#### Step 3: Compile the C Shell Engine

```powershell
wsl -e bash -c "mkdir -p ~/retroshell && cp retroshell.c ~/retroshell/ && cd ~/retroshell && gcc -std=c11 -Wall -Wextra -o retroshell retroshell.c"
```

#### Step 4: Install Node Dependencies

```powershell
npm install
```

#### Step 5: Launch

Double-click `launch.bat` in the project folder, or run:

```powershell
npm start
```

#### Optional: Global Hotkey Setup

1. Right-click `launch.vbs` in the project folder and choose **Create Shortcut**.
2. Move the shortcut to your Desktop.
3. Right-click the shortcut → **Properties** → **Shortcut key** field.
4. Press your desired key combination (e.g. `Ctrl+Alt+3`).
5. Click **Apply** and **OK**.
6. Sign out and back in for the hotkey to activate.

---

### Linux / macOS — Native CLI Mode

```bash
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
make
./retroshell
```

Or compile manually:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -o retroshell retroshell.c
./retroshell
```

> On Linux and macOS, `retroshell` runs as a full-featured POSIX shell. The Electron GUI requires a Windows host with WSL2 to function.

---

## 9. Project Structure

```
RetroOS_Shell/
├── retroshell.c          # Core shell engine (C11): tokenizer, evaluator, scheduler, monitor
├── main.js               # Electron main process: PTY management, IPC bridge, window control
├── preload.js            # Electron preload: secure contextBridge IPC exposure
├── index.html            # Terminal UI: xterm.js, titlebar, font controls, clipboard handling
├── launch.bat            # Windows launcher: sets environment and starts Electron
├── launch.vbs            # Silent launcher: runs Electron without a CMD flash (for hotkeys)
├── package.json          # Node.js project manifest (version 1.3.0)
├── LICENSE               # MIT License
├── Makefile              # Build rules for native Linux/macOS compilation
├── fonts/
│   ├── Regular.ttf       # RetroNerd Mono Regular
│   └── Bold.ttf          # RetroNerd Mono Bold
└── icon.png              # Application icon
```

---

## 10. Academic Context

RetroOS Terminal was developed as an operating systems course project. The following concepts are demonstrated with real, working implementations:

| OS Concept | Implementation |
| :--- | :--- |
| **Process Creation** | `fork()` + `execvp()` for every external command |
| **Pipelines** | Multi-stage pipe chains (`cmd1 \| cmd2 \| cmd3`) using `pipe()` and `dup2()` |
| **I/O Redirection** | `<`, `>`, `>>`, `2>`, `2>>`, `2>&1`, `1>&2`, `&>` |
| **Job Control** | Asynchronous background jobs (`&`), `jobs` listing, `SIGCHLD` reaping |
| **CPU Scheduling** | Simulated FCFS with Gantt chart + preemptive Round Robin on real child processes |
| **Resource Monitoring** | `/proc/stat` CPU sampling, `/proc/meminfo` RAM/swap, `/proc/[pid]/stat` per-process stats |
| **Filesystem Abstraction** | Dynamic detection of WSL2 DrvFs mounts, VFS path mapping |
| **Signal Handling** | `SIGINT`, `SIGCHLD`, `SIGWINCH` (terminal resize) |
| **Privilege Levels** | Simulated Ring 0 / Ring 3 mode toggle with prompt indicator |

---

## 11. Project Status & Terms

This project is an **academic coursework project** built for educational purposes and personal demonstration. All rights reserved by the author. You are welcome to review, test, and learn from the codebase for educational and non-commercial purposes.

---

<div align="center">

Engineered by [SaKin32](https://github.com/SaKin32)

*"The shell is not a tool. It is your will, made executable."*

</div>
