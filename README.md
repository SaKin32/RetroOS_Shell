# RetroOS Terminal

<div align="center">

![C11](https://img.shields.io/badge/C-C11%20Standard-blue?style=for-the-badge&logo=c)
![Electron](https://img.shields.io/badge/GUI-Electron%20v33%20%2B%20xterm.js-47848F?style=for-the-badge&logo=electron)
![Node](https://img.shields.io/badge/Node.js-18%2B-339933?style=for-the-badge&logo=nodedotjs)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-blueviolet?style=for-the-badge&logo=linux)
![License](https://img.shields.io/badge/License-MIT-ec4899?style=for-the-badge)
![Stars](https://img.shields.io/github/stars/SaKin32/RetroOS_Shell?style=for-the-badge&color=facc15)

**A Next-Generation Cyberpunk Desktop Terminal Environment**

*Custom C Shell Engine · Dual Unix & Windows Command Interop · Braille Halftone Fastfetch*
*CPU Process Scheduler · Powerline Prompt · Multi-platform Electron GUI*

</div>

---

## Screenshots

<div align="center">

### First-Time Onboarding & Profile Setup
> *The RetroOS ASCII banner greets the user and prompts them to set up a personalized shell identity.*

![Welcome Screen](screenshots/t1.png)

---

### Interactive Cyber Purple Shell Session
> *Instant transition to the shell — featuring the 18-line Braille Cyber Purple Cat, live hardware stats, and a powerline prompt.*

![Shell Session](screenshots/t2.png)

</div>

---

## Overview: Dual Command Workflow (Linux OS + Windows OS)

RetroOS Terminal bridges Unix and Windows environments. It runs an interactive C11 shell with an Electron-powered front end while allowing seamless execution of both native Unix/Linux commands and Windows applications and commands in the same session.

```
+-----------------------------------------------------------------------+
|                         RetroOS Terminal GUI                          |
|         (xterm.js + Electron frameless OLED canvas + Font Zoom)       |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                    retroshell (Interactive C Shell)                   |
|     Pipelines | Redirection | Job Control | Scheduler | Monitor       |
+-----------------------------------------------------------------------+
             |                                              |
             v                                              v
+--------------------------+                  +-------------------------+
|     Unix / OS Engine     |                  |   Windows Interop Layer |
| ls, grep, cat, make, gcc |                  | explorer.exe, code, git |
| sched, monitor, pipes    |                  | powershell.exe, cmd.exe |
+--------------------------+                  +-------------------------+
```

---

## How to Use Windows & OS Commands Together

Once RetroOS Terminal launches, the default working directory is set to your Windows User Home (for example, `C:\Users\<username>`). You can execute Unix terminal utilities, RetroOS built-in commands, and Windows `.exe` tools interchangeably.

### 1. Navigating Windows Drives and Directories

Windows drives are mounted under `/mnt/` inside the shell environment:

| Windows Location | Command in RetroOS Shell |
| :--- | :--- |
| Your Windows User Profile | `cd ~` or `cd /mnt/c/Users/<your_username>` |
| Root of Drive C: | `cd /mnt/c` |
| Root of Drive E: | `cd /mnt/e` |
| View current directory files | `ls -la` |
| Print working directory path | `pwd` |

### 2. Running Windows Applications & GUI Tools

You can launch native Windows software directly from the command prompt:

```bash
# Open current directory in Windows File Explorer
explorer.exe .

# Open a specific file in Windows Notepad
notepad.exe notes.txt

# Open the current folder in Visual Studio Code
code .

# Launch Windows Task Manager
taskmgr.exe

# Open Windows Calculator
calc.exe
```

### 3. Running Windows Developer & CLI Tools

Tools installed in Windows (such as Git, Python, Node.js, and Docker) can be invoked directly by adding `.exe` or via their standard names:

```bash
# Windows Git version & status
git.exe status
git.exe log --oneline -5

# Windows Python runtime
python.exe script.py

# Windows Node & NPM
node.exe -v
npm.cmd -v

# Windows Docker CLI
docker.exe ps
```

### 4. Running Native Windows PowerShell and CMD Commands

If you need native Windows commands (such as `ipconfig`, `systeminfo`, or PowerShell cmdlets), run them through `cmd.exe` or `powershell.exe`:

```bash
# Check network configuration using Windows ipconfig
cmd.exe /c ipconfig

# Check running Windows processes
cmd.exe /c tasklist

# Execute any PowerShell command or script
powershell.exe -Command "Get-Service | Where-Object {$_.Status -eq 'Running'}"
powershell.exe -Command "Get-Process | Sort-Object CPU -Descending | Select-Object -First 10"

# Open interactive Windows PowerShell session inside RetroOS
powershell.exe

# Open interactive Windows CMD session inside RetroOS
cmd.exe
```

### 5. Running Native Unix / Linux OS Commands

All standard Unix tools and RetroOS features run natively:

```bash
# File inspection and text processing
cat package.json | grep version
find . -name "*.js" -type f

# System resource monitor
free -m
uptime

# Chained pipelines and redirection
ls -lh | sort -k 5 -h
cat /proc/cpuinfo | grep "model name" | uniq
```

### 6. Mixing Unix Utilities with Windows Output (Piping & Filtering)

You can pass output from Windows binaries directly into Unix text utilities:

```bash
# Filter Windows tasklist through Unix grep
cmd.exe /c tasklist | grep -i "electron"

# Search Windows IP configuration with grep
cmd.exe /c ipconfig | grep -i "IPv4"

# Sort Windows directory listings using Unix utilities
cmd.exe /c dir | grep -E "(DIR)"
```

---

## Feature Highlights

| Category | Feature |
| :--- | :--- |
| **Dual OS Workflow** | Run native Unix commands and Windows executables simultaneously |
| **Shell Engine** | Full POSIX-style C shell: fork/exec, pipelines, redirection, background jobs, globbing |
| **Process Scheduler** | FCFS & Round Robin CPU scheduler with ASCII Gantt chart; real benchmark mode |
| **System Monitor** | Live CPU, RAM, and process stats dashboard (`monitor`) |
| **Fastfetch Banner** | 18-line Braille halftone Cyber Purple Cat + hardware analytics tree |
| **User Identity** | Personalized `username@retros` prompt, stored in `~/.retroshell_user` |
| **Desktop GUI** | Electron v33 + xterm.js — OLED black canvas, custom titlebar, font zoom |
| **Font Zoom** | `Ctrl + =` / `Ctrl + -` / `Ctrl + 0` and `Ctrl + MouseWheel` |
| **Themes** | `retro` · `cyber` · `matrix` · `dracula` |
| **Kernel Mode** | Simulated Ring 3 to Ring 0 privilege switcher (`mode-switch`) |

---

## Installation & Setup

Choose your platform below.

---

### Windows (Recommended Full Dual-OS Experience)

<details>
<summary><b>Expand Windows Setup Guide</b></summary>

#### Prerequisites

| Tool | Download |
| :--- | :--- |
| WSL 2 (Windows Subsystem for Linux) | [Microsoft Docs](https://learn.microsoft.com/en-us/windows/wsl/install) |
| Ubuntu (via WSL) | Microsoft Store — search "Ubuntu" |
| Node.js 18+ | [nodejs.org](https://nodejs.org/) |
| Git | [git-scm.com](https://git-scm.com/) |

#### Step 1 — Install WSL (if not already installed)

Open **PowerShell as Administrator** and run:

```powershell
wsl --install
```

Restart your computer when prompted, then complete your Ubuntu username/password setup.

#### Step 2 — Clone the repository

```powershell
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
```

#### Step 3 — Compile the C Shell binary inside WSL

```powershell
wsl -e bash -c "cd ~/retroshell && gcc -std=c11 -Wall -o retroshell retroshell.c"
```

> If `~/retroshell` does not exist in WSL, copy `retroshell.c` to it:
> ```powershell
> wsl -e bash -c "mkdir -p ~/retroshell"
> wsl -e bash -c "cp /mnt/e/retroos-terminal/retroshell.c ~/retroshell/"
> ```

#### Step 4 — Install dependencies

```powershell
npm install
```

#### Step 5 — Launch the application

Double-click `launch.bat` or run:

```powershell
npm start
```

</details>

---

### macOS

<details>
<summary><b>Expand macOS Setup Guide</b></summary>

#### Prerequisites

```bash
xcode-select --install
brew install node
```

#### Setup & Launch

```bash
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell

# Compile C binary
make

# Install Node modules & launch
npm install
npm start
```

</details>

---

### Linux

<details>
<summary><b>Expand Linux Setup Guide</b></summary>

#### Prerequisites (Ubuntu / Debian / Mint)

```bash
sudo apt update && sudo apt install -y build-essential gcc make git curl
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
```

#### Setup & Launch

```bash
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell

make
npm install
npm start
```

</details>

---

### Terminal-Only Mode (No GUI)

To run the raw C shell directly in any Linux, WSL, or macOS terminal:

```bash
./retroshell
```

---

## Built-in Command Reference

| Command | Syntax | Description |
| :--- | :--- | :--- |
| `help` | `help` | List all available shell commands |
| `welcome` | `welcome` | Revisit the welcome and onboarding screen |
| `user` | `user [name]` | View or update your `@retros` identity |
| `theme` | `theme <name>` | Switch accent theme: `retro` · `cyber` · `matrix` · `dracula` |
| `monitor` | `monitor [sec]` | Real-time CPU, RAM and process dashboard |
| `sched` | `sched fcfs 0:5 1:3` | FCFS CPU scheduler with ASCII Gantt chart |
| `sched real` | `sched real rr 0.5 "burn 2"` | Live preemptive Round Robin benchmark |
| `mode-switch` | `mode-switch` | Toggle Ring 3 User to Ring 0 Kernel mode |
| `clear` | `clear` | Clear the terminal screen |
| `exit` | `exit` | Exit the retroshell session |

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
├── retroshell.c       # Core C shell: lexer, parser, fork/exec, scheduler, fastfetch
├── Makefile           # Compilation rules for C shell
├── main.js            # Electron main process: window manager, PTY bridge, hotkeys
├── index.html         # Frontend GUI: xterm.js terminal, titlebar, theme styling
├── icon.png           # Cyberpunk purple terminal icon (PNG)
├── icon.ico           # Windows multi-size application icon (ICO)
├── launch.bat         # Windows safe launcher script
├── fonts/             # Nerd Fonts for powerline glyphs and terminal rendering
├── screenshots/       # Documentation showcase images
└── package.json       # Electron dependencies and startup scripts
```

---

## Troubleshooting

| Issue | Resolution |
| :--- | :--- |
| `retroshell: No such file or directory` | Build the binary first: `gcc -std=c11 -Wall -o retroshell retroshell.c` |
| Window closes instantly on launch | Verify WSL is operational and the binary exists in `~/retroshell/retroshell` |
| Powerline symbols show as boxes | Install a Nerd Font or launch using `./retroshell --ascii` |
| `Cannot find module 'electron'` | Run `npm install` inside the project root folder |
| Permission errors on Windows drives | Run terminal as your regular Windows user account |

---

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit your changes: `git commit -m "Add my feature"`
4. Push to the branch: `git push origin feature/my-feature`
5. Open a Pull Request

---

## License

Licensed under the **[MIT License](LICENSE)**.

---

<div align="center">

Made with passion by [SaKin32](https://github.com/SaKin32)

*"The shell is not a tool. It is your will, made executable."*

</div>
