# RetroOS Terminal

<div align="center">

![C11](https://img.shields.io/badge/Language-C11%20Standard-blue?style=for-the-badge&logo=c)
![Electron](https://img.shields.io/badge/GUI-Electron%20v33%20%2B%20xterm.js-47848F?style=for-the-badge&logo=electron)
![Platform](https://img.shields.io/badge/Primary%20Platform-Windows%2011%20(WSL2)-0078D6?style=for-the-badge&logo=windows)
![CLI](https://img.shields.io/badge/CLI%20Platform-Linux%20%7C%20macOS-339933?style=for-the-badge&logo=linux)
![License](https://img.shields.io/badge/License-MIT-ec4899?style=for-the-badge)

**A Cyberpunk Desktop Terminal & Educational Unix/Windows Hybrid Shell**

*POSIX Unix Core · Dynamic OS Boot Selector · Windows Command Interop Bridge*
*Braille Halftone Fastfetch · CPU Process Scheduler · Dynamic VFS Mapper*

</div>

---

## 1. Project Purpose & Academic Context

RetroOS Terminal was developed as an operating systems course project demonstrating core systems concepts:

- **Process Lifecycle & Multiprocessing**: Custom fork/exec pipeline dispatcher with asynchronous job control (`&`, `jobs`).
- **CPU Scheduling Algorithms**: Simulated FCFS with ASCII Gantt chart generation (`sched fcfs`) and preemptive Round Robin benchmarking on real child processes (`sched real rr`).
- **Resource Monitoring**: Live `/proc` metrics engine (`monitor`) displaying CPU percentages, virtual memory statistics, and process states.
- **Dual OS Interoperability**: Explores bridging a Unix POSIX environment with a host Windows OS (WSL2 / DrvFs mount points, Windows drive switching, and executable dispatching).

---

## 2. Technical Architecture & Honesty in Design

It is important to understand how RetroOS executes commands across systems:

```
+---------------------------------------------------------------------------------------+
|                                  ELECTRON GUI LAYER                                   |
|                xterm.js Canvas + Frameless Titlebar + Zoom Engine (12-28px)           |
+---------------------------------------------------------------------------------------+
                                           |
                                      [node-pty]
                                           |
                                           v
+---------------------------------------------------------------------------------------+
|                              RETROSHELL ENGINE (C11)                                  |
|                                                                                       |
|   1. POSIX Core Commands  -> Native fork(), execvp(), pipes, redirection               |
|   2. CMD Aliases          -> dir (ls -lah), cls (clear), type (cat), md, rd, del       |
|   3. Drive Switcher       -> C:, E:, D: dynamically resolved via /mnt/<drive>         |
|   4. Dynamic Dispatcher   -> Resolves .exe & /mnt/c/Windows/System32 automatically     |
+---------------------------------------------------------------------------------------+
                         |                                           |
                         v                                           v
+--------------------------------------------------+ +----------------------------------+
|               LINUX ENVIRONMENT                  | |          WINDOWS SUBSYSTEM       |
|   ext4 rootfs (/), /proc, gcc, make, coreutils   | |   NTFS /mnt/c & /mnt/e, Explorer |
|   Native Linux child processes                   | |   Windows binaries dispatched    |
+--------------------------------------------------+ +----------------------------------+
```

### Clarification on Platform Support:
- **Full Desktop GUI (Electron + retroshell)**: Supported on **Windows 10/11 using WSL2**.
- **Native Linux / macOS**: Supports compiling and running the native CLI shell (`./retroshell`) and developer builds.
- **Windows Command Execution**: Windows commands (`ipconfig`, `systeminfo`, `tasklist`, `explorer`) are executed via WSL2 host interop by dispatching to the underlying Windows binaries in `/mnt/c/Windows/System32`.

---

## 3. Dynamic Environment Boot Selector

When launching RetroOS Terminal, an interactive boot loader configures your initial working directory and prompt focus:

```text
  +-----------------------------------------------------------------------------+
  |                      R E T R O   O S   B O O T   L O A D E R                |
  |                      Select Operating Execution Environment                 |
  +-----------------------------------------------------------------------------+
    [1] Windows Engine Mode    -> Defaults to detected Windows User Profile
    [2] Linux Unix Engine Mode -> Defaults to Linux ~/home directory
    [3] Hybrid Mode (Default)  -> Dual interop (Windows drives + Unix tools)
  +-----------------------------------------------------------------------------+
```

### In-Session Mode Switching:
You can switch modes without restarting:
- `os win` : Switches active directory to the host Windows User profile.
- `os linux` : Switches active directory to the Linux user home (`~`).
- `os hybrid` : Enables seamless dual compatibility.
- `os` : Prints active mode diagnostics.

---

## 4. Real-time Filesystem Mapping (`fs` or `drives`)

Execute `fs` or `drives` to inspect dynamically detected host partitions and Unix virtual filesystems:

```text
  ================== DUAL OS FILESYSTEM MAPPING ==================

  [DETECTED WINDOWS HOST PARTITIONS]
  Drive Letter    WSL Mount Point   Filesystem Type   Status
  ------------    ---------------   ---------------   ------
  C:\             /mnt/c            DrvFs / 9p        Active (Accessible)
  E:\             /mnt/e            DrvFs / 9p        Active (Accessible)

  [LINUX UNIX VIRTUAL FILESYSTEMS (VFS)]
  Virtual Path    Target Storage    Purpose
  ------------    --------------    -------
  /               rootfs            Unix Root Environment
  ~ (/home/...)   ext4 user home    User Home Directory & Configs
  /proc           procfs (RAM)      Kernel Virtual Process Tree & Metrics
  /sys            sysfs (RAM)       Device Drivers & Kernel Subsystems
```

---

## 5. Command Reference

### Windows CMD Aliases & Interop Tools
| Command | Action | Implementation |
| :--- | :--- | :--- |
| `C:` or `E:` or `D:` | Switch to target Windows drive | Built-in `chdir()` to `/mnt/<drive>` |
| `dir` | List files and directories | Invokes `ls -lah --color=auto` |
| `cls` | Clear terminal display | Clears canvas and purge scrollback buffer |
| `type <file>` | Display text file contents | Maps to Unix `cat` |
| `md <folder>` | Create directory | Maps to `mkdir -p` |
| `rd <folder>` | Remove directory | Maps to `rmdir` |
| `del <file>` | Delete file | Maps to `rm -f` |
| `copy` / `move` / `ren` | Copy, move, or rename files | Maps to `cp -r` and `mv` |
| `ipconfig` | Display Windows network adapters | Dispatches to `System32/ipconfig.exe` |
| `tasklist` | Display Windows process table | Dispatches to `System32/tasklist.exe` |
| `systeminfo` | Detailed Windows host diagnostics | Dispatches to `System32/systeminfo.exe` |
| `explorer .` | Open Windows Explorer | Dispatches to Windows Explorer executable |
| `code .`, `notepad` | Open GUI applications | Dispatches to host Windows binaries |

### Academic & Core OS Features
| Command | Example | Description |
| :--- | :--- | :--- |
| `sched fcfs` | `sched fcfs 0:5 1:3 2:8` | Simulated FCFS CPU scheduler with ASCII Gantt chart |
| `sched real` | `sched real rr 0.5 "burn 2"` | Real preemptive Round Robin scheduling benchmark |
| `monitor` | `monitor 1` | Live process state, CPU, and RAM metrics parsed from `/proc` |
| `mode-switch`| `mode-switch` | Simulated privilege level toggle between Ring 3 (User) and Ring 0 (Kernel) |
| `jobs` | `jobs` | Table of active background asynchronous child processes |

---

## 6. Installation & Execution Guide

### Windows 10 / 11 (Full Desktop GUI)

#### Step 1: Install WSL 2 (if not already installed)
Open PowerShell as Administrator:
```powershell
wsl --install
```

#### Step 2: Clone the Repository
```powershell
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
```

#### Step 3: Build the C Shell
Compile the core engine into your WSL environment:
```powershell
wsl -e bash -c "mkdir -p ~/retroshell && cp retroshell.c ~/retroshell/ && cd ~/retroshell && gcc -std=c11 -Wall -o retroshell retroshell.c"
```

#### Step 4: Install Dependencies & Run
```powershell
npm install
.\launch.bat
```
*(Or launch directly using `npm start`).*

---

### Linux / macOS (Native CLI Mode)

You can compile and run `retroshell` directly as a native Unix shell:

```bash
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
make
./retroshell
```

---

## 7. Keyboard Shortcuts

| Shortcut | Scope | Action |
| :--- | :--- | :--- |
| `Ctrl` + `Alt` + `T` | Global (Windows) | Summon or focus RetroOS Terminal |
| `Ctrl` + `=` | Terminal Window | Zoom In font size (+1px) |
| `Ctrl` + `-` | Terminal Window | Zoom Out font size (-1px) |
| `Ctrl` + `0` | Terminal Window | Reset font size to default (15px) |
| `Ctrl` + Mouse Wheel | Terminal Window | Smooth interactive font scaling |
| `Ctrl` + `Shift` + `N` | Terminal Window | Spawn new session window |

---

## 8. License

This project is licensed under the **[MIT License](LICENSE)**.

---

<div align="center">

Engineered by [SaKin32](https://github.com/SaKin32)

*"The shell is not a tool. It is your will, made executable."*

</div>
