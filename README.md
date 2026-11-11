# RetroOS Terminal

<div align="center">

![C11](https://img.shields.io/badge/C-C11%20Standard-blue?style=for-the-badge&logo=c)
![Electron](https://img.shields.io/badge/GUI-Electron%20v33%20%2B%20xterm.js-47848F?style=for-the-badge&logo=electron)
![Node](https://img.shields.io/badge/Node.js-18%2B-339933?style=for-the-badge&logo=nodedotjs)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-blueviolet?style=for-the-badge&logo=linux)
![License](https://img.shields.io/badge/License-MIT-ec4899?style=for-the-badge)
![Stars](https://img.shields.io/github/stars/SaKin32/RetroOS_Shell?style=for-the-badge&color=facc15)

**A Next-Generation Cyberpunk Desktop Terminal Environment**

*Interactive Dual OS Boot Selector · Windows CMD & Linux POSIX Engines*
*Dual VFS Architecture · Braille Halftone Fastfetch · CPU Process Scheduler*

</div>

---

## Academic & Engineering Showcase for Reviewers

RetroOS Terminal is designed as an interactive hybrid operating system interface demonstrating modern OS principles:

1. **Dual OS Execution Engine**: Switch between pure Windows CMD workflows, Unix/Linux POSIX shell workflows, or seamless Hybrid Mode.
2. **Virtual Filesystem (VFS) Abstraction**: Visualizes how Windows host drives (`C:\`, `E:\`) and Unix virtual mounts (`/`, `/proc`, `/sys`) coexist.
3. **Core OS Features**: Custom fork/exec pipeline engine, interactive process scheduling (FCFS, Round Robin with ASCII Gantt charts), live `/proc` process monitor, and ring privilege level toggling.

---

## Operating Modes (Windows vs. Linux vs. Hybrid)

Upon launching RetroOS Terminal, the interactive **Boot Loader** allows you to choose your environment:

```text
  ┌────────────────────────────────────────────────────────────────────────┐
  │              R E T R O   O S   B O O T   L O A D E R                   │
  │              Select your Operating Execution Environment               │
  ├────────────────────────────────────────────────────────────────────────┤
  │  [1] Windows Engine Mode    (C:\ drive, Windows CMD, native .exe tools)│
  │  [2] Linux Unix Engine Mode (~/home, POSIX shell, schedulers, Linux)   │
  │  [3] Hybrid Interop Mode    (Seamless dual compatibility - Default)    │
  └────────────────────────────────────────────────────────────────────────┘
```

You can also switch environments at any time without restarting the terminal:

```bash
# Switch to Windows Environment
os win

# Switch to Linux Unix Environment
os linux

# Switch to Hybrid Mode
os hybrid

# Inspect active environment status
os
```

---

## Dual Filesystem Architecture (`fs` or `drives`)

Typing `fs` or `drives` inside RetroOS displays the real-time mapping of Windows and Linux storage:

```text
  ================== DUAL OS FILESYSTEM MAPPING ==================

  [WINDOWS HOST PARTITIONS]
  Logical Drive   WSL Mount Point   Filesystem Type   Status
  -------------   ---------------   ---------------   ------
  C:\             /mnt/c            NTFS / 9p         Active (System OS)
  E:\             /mnt/e            NTFS / 9p         Active (RetroOS Repo)

  [LINUX UNIX VIRTUAL FILESYSTEMS (VFS)]
  Virtual Path    Target Storage    Purpose
  ------------    --------------    -------
  /               ext4 rootfs       Unix Root System
  ~ (/home/...)   ext4 user home    Unix User Configs & retroshell
  /proc           procfs (memory)   Kernel Process Tree & Live Stats
  /sys            sysfs (kernel)    Hardware & Driver Subsystems
```

---

## Command Reference: Windows vs. Linux

| Category | Windows Command | Linux Equivalent | Description |
| :--- | :--- | :--- | :--- |
| **Switch Drive** | `C:` or `E:` | `cd /mnt/c` or `cd /mnt/e` | Direct drive navigation |
| **List Files** | `dir` | `ls -lah` | Lists files with permissions and size |
| **Clear Display** | `cls` | `clear` | Clears terminal canvas |
| **Read File** | `type <file>` | `cat <file>` | Prints file contents |
| **Make Directory** | `md <name>` | `mkdir -p <name>` | Creates directory |
| **Remove Directory**| `rd <name>` | `rmdir <name>` | Removes directory |
| **Delete File** | `del <file>` | `rm -f <file>` | Deletes file |
| **Copy File** | `copy <src> <dst>` | `cp -r <src> <dst>` | Copies files or folders |
| **Move / Rename** | `move` / `ren` | `mv` | Moves or renames target |
| **Inspect Drives**| `fs` / `drives` | `df -h` | Displays dual OS storage map |
| **Switch OS Mode**| `os win` | `os linux` | Changes prompt and execution focus |
| **Launch Apps** | `explorer .`, `code .`, `notepad` | — | Direct execution without `.exe` |
| **Networking** | `ipconfig` | `ifconfig` / `ip a` | Displays IP and network info |
| **Task Manager** | `tasklist` | `ps aux` | Shows running OS processes |

---

## Core OS Academic Built-ins

| Command | Usage | OS Concept Demonstrated |
| :--- | :--- | :--- |
| `sched` | `sched fcfs 0:5 1:3 2:8` | Non-preemptive First-Come-First-Served CPU scheduler with Gantt chart |
| `sched real` | `sched real rr 0.5 "burn 2"` | Preemptive Round Robin scheduling on real child processes |
| `monitor` | `monitor 1` | Live process state, CPU, and RAM metrics parsed from `/proc` |
| `mode-switch` | `mode-switch` | Simulated CPU privilege level escalation (Ring 3 User ↔ Ring 0 Kernel) |
| `jobs` | `jobs` | Background job table and asynchronous process monitoring |

---

## Installation & Setup

### Windows Setup

1. **Prerequisites**: Ensure WSL 2 with Ubuntu is enabled and Node.js 18+ is installed.
2. **Build the C Shell Binary**:
   ```powershell
   wsl -e bash -c "mkdir -p ~/retroshell && cp retroshell.c ~/retroshell/ && cd ~/retroshell && gcc -std=c11 -Wall -o retroshell retroshell.c"
   ```
3. **Install Dependencies**:
   ```powershell
   npm install
   ```
4. **Launch**:
   ```powershell
   .\launch.bat
   # or
   npm start
   ```

### Linux & macOS Setup

```bash
make
npm install
npm start
```

---

## Desktop GUI Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl` + `=` | Zoom In Font Size |
| `Ctrl` + `-` | Zoom Out Font Size |
| `Ctrl` + `0` | Reset Zoom to Default |
| `Ctrl` + Mouse Wheel | Smooth Font Zoom In / Out |
| Titlebar `[A-]` / `[A+]` | Quick click-to-zoom controls |

---

## Project Structure

```
RetroOS_Shell/
├── retroshell.c       # Core hybrid C shell engine (VFS mapper, boot loader, scheduler)
├── Makefile           # Compilation rules for C binary
├── main.js            # Electron main process (PTY bridge, hotkeys, window manager)
├── index.html         # Frontend GUI (xterm.js OLED terminal canvas, custom titlebar)
├── icon.png           # Cyberpunk purple terminal icon (PNG)
├── icon.ico           # Windows multi-size application icon (ICO)
├── launch.bat         # Windows safe launcher script
├── fonts/             # Nerd Fonts for powerline glyphs and terminal rendering
├── screenshots/       # Documentation showcase images
└── package.json       # Electron dependencies and startup scripts
```

---

## License

Licensed under the **[MIT License](LICENSE)**.

---

<div align="center">

Made with passion by [SaKin32](https://github.com/SaKin32)

*"The shell is not a tool. It is your will, made executable."*

</div>
