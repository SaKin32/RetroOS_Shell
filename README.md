# RetroOS Terminal

<div align="center">

![C11](https://img.shields.io/badge/C-C11%20Standard-blue?style=for-the-badge&logo=c)
![Electron](https://img.shields.io/badge/GUI-Electron%20v33%20%2B%20xterm.js-47848F?style=for-the-badge&logo=electron)
![Node](https://img.shields.io/badge/Node.js-18%2B-339933?style=for-the-badge&logo=nodedotjs)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux-blueviolet?style=for-the-badge&logo=linux)
![License](https://img.shields.io/badge/License-MIT-ec4899?style=for-the-badge)
![Stars](https://img.shields.io/github/stars/SaKin32/RetroOS_Shell?style=for-the-badge&color=facc15)

**A Next-Generation Cyberpunk Desktop Terminal Environment**

*Native Windows CMD & Linux OS Hybrid Architecture · Braille Halftone Fastfetch*
*Direct Drive Switching · CPU Process Scheduler · Powerline Prompt · Electron OLED Canvas*

</div>

---

## Screenshots

<div align="center">

### First-Time Onboarding & Profile Setup
> *The RetroOS ASCII banner greets the user and prompts them to set up a personalized shell identity.*

![Welcome Screen](screenshots/t1.png)

---

### Interactive Cyber Purple Shell Session (Windows Prompt)
> *Displays paths in authentic Windows style (`C:\Users\...` or `E:\...`) with the 18-line Braille Cyber Purple Cat and hardware analytics.*

![Shell Session](screenshots/t2.png)

</div>

---

## Architecture: Windows CMD + Linux OS Dual Compatibility

RetroOS Terminal combines the power of a POSIX Unix shell engine with the familiar commands and path styling of Windows Command Prompt (CMD) and PowerShell.

```
+-------------------------------------------------------------------------------+
|                             RetroOS Terminal GUI                              |
|           (Frameless OLED canvas + Font Zoom + Cyberpunk Titlebar)            |
+-------------------------------------------------------------------------------+
                                        |
                                        v
+-------------------------------------------------------------------------------+
|                       retroshell (Hybrid C Shell Engine)                      |
|                                                                               |
|   Windows Prompt Formatter:  C:\Users\<user>  |  E:\project                   |
|   Direct Drive Switching:    C:  |  E:  |  D:                                 |
|   Windows CMD Aliases:       dir, cls, type, md, rd, del, copy, move, ren    |
|   POSIX Unix Core:           pipes (|), redirection (< >), jobs (&), sched    |
|   Automatic .exe Dispatcher: notepad, explorer, code, tasklist, ipconfig       |
+-------------------------------------------------------------------------------+
                  |                                             |
                  v                                             v
     +--------------------------+                  +-------------------------+
     |   Native Unix Tools      |                  |   Windows Application   |
     | grep, cat, make, gcc, ls |                  | Explorer, VS Code, Git  |
     | FCFS & RR Schedulers     |                  | PowerShell & CMD Tools  |
     +--------------------------+                  +-------------------------+
```

---

## Windows CMD Command Reference & Quick Guide

You do not need to learn separate commands. You can use standard Windows CMD commands, PowerShell commands, and Linux utilities interchangeably in the same session.

### 1. Drive Switching & Windows Paths

Switch drives directly like Windows CMD:

```cmd
> E:
# Automatically switches prompt to E:\

> C:
# Automatically switches prompt to C:\
```

Paths in the prompt are automatically formatted in standard Windows syntax:
```text
comando_32@retros  C:\Users\mdsho  >
comando_32@retros  E:\retroos-terminal  >
```

### 2. Windows CMD File and Directory Operations

| Action | Windows CMD Command | Linux Equivalent | Result |
| :--- | :--- | :--- | :--- |
| Clear screen | `cls` | `clear` | Clears terminal canvas |
| List files | `dir` | `ls -lah` | Lists files and directories with colors |
| View file text | `type README.md` | `cat README.md` | Prints file contents to screen |
| Create directory | `md my_folder` | `mkdir -p my_folder` | Creates folder |
| Remove directory | `rd my_folder` | `rmdir my_folder` | Deletes directory |
| Delete file | `del file.txt` | `rm -f file.txt` | Removes file |
| Copy files | `copy a.txt b.txt` | `cp -r a.txt b.txt` | Copies file or directory |
| Rename / Move | `ren old.txt new.txt` | `mv old.txt new.txt` | Renames or moves file |
| Navigate directory | `cd ..` or `cd \` | `cd ..` | Changes current directory |

### 3. Launching Windows Software (No .exe Required)

Windows software and GUI applications can be launched directly without typing `.exe`:

```bash
# Open current directory in Windows File Explorer
explorer .

# Open file in Windows Notepad
notepad notes.txt

# Launch Visual Studio Code
code .

# Launch Windows Task Manager
taskmgr

# Open Windows Calculator
calc
```

### 4. Windows Networking & System Diagnostics

Run native Windows administrative tools directly from the prompt:

```bash
# Display network configuration
ipconfig

# Detailed system summary
systeminfo

# List running Windows processes
tasklist

# Terminate process by name or PID
taskkill /IM notepad.exe /F

# Ping host
ping google.com
```

### 5. Advanced Hybrid Workflows: Windows + Linux Pipes

Combine Windows system commands with Unix text-filtering utilities:

```bash
# Filter Windows process list using Unix grep
tasklist | grep -i electron

# Find specific network adapters from ipconfig
ipconfig | grep -i IPv4

# Search inside file listing
dir | grep -i "\.json"
```

---

## Built-in Shell Commands Reference

| Command | Syntax | Description |
| :--- | :--- | :--- |
| `help` | `help` | Display list of all available commands |
| `welcome` | `welcome` | Open the interactive welcome and profile screen |
| `user` | `user [name]` | View or update your `@retros` shell profile |
| `theme` | `theme <name>` | Switch theme: `retro`, `cyber`, `matrix`, `dracula` |
| `monitor` | `monitor [sec]` | Real-time CPU, RAM, and process activity monitor |
| `sched` | `sched fcfs 0:5 1:3` | Run FCFS CPU scheduling with ASCII Gantt chart |
| `sched real` | `sched real rr 0.5 "burn 2"` | Run real preemptive Round Robin benchmark |
| `mode-switch` | `mode-switch` | Toggle simulated Ring 3 User and Ring 0 Kernel modes |
| `cls` / `clear` | `cls` | Clear terminal display |
| `exit` | `exit` | Gracefully terminate shell session |

---

## Installation & Setup

Choose your operating system below.

---

### Windows (Full Hybrid Experience)

<details>
<summary><b>Expand Windows Setup Guide</b></summary>

#### Prerequisites

| Tool | Link / Guide |
| :--- | :--- |
| WSL 2 (Windows Subsystem for Linux) | [Microsoft Documentation](https://learn.microsoft.com/en-us/windows/wsl/install) |
| Ubuntu (via WSL) | Microsoft Store — search "Ubuntu" |
| Node.js 18+ | [nodejs.org](https://nodejs.org/) |
| Git | [git-scm.com](https://git-scm.com/) |

#### Step 1 — Install WSL (if not already installed)

Open **PowerShell as Administrator**:

```powershell
wsl --install
```

Restart your computer if prompted and complete your Ubuntu username/password setup.

#### Step 2 — Clone the Repository

```powershell
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
```

#### Step 3 — Compile the C Shell binary in WSL

```powershell
wsl -e bash -c "mkdir -p ~/retroshell && cp retroshell.c ~/retroshell/ && cd ~/retroshell && gcc -std=c11 -Wall -o retroshell retroshell.c"
```

#### Step 4 — Install Node.js dependencies

```powershell
npm install
```

#### Step 5 — Launch the application

Launch via the safe Windows batch file or npm:

```powershell
.\launch.bat
# or
npm start
```

</details>

---

### macOS

<details>
<summary><b>Expand macOS Setup Guide</b></summary>

```bash
# Install dependencies
xcode-select --install
brew install node

# Clone and compile
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
make

# Install Node modules & start
npm install
npm start
```

</details>

---

### Linux

<details>
<summary><b>Expand Linux Setup Guide</b></summary>

```bash
# Ubuntu / Debian
sudo apt update && sudo apt install -y build-essential gcc make git nodejs npm

# Clone and compile
git clone https://github.com/SaKin32/RetroOS_Shell.git
cd RetroOS_Shell
make

# Install Node modules & start
npm install
npm start
```

</details>

---

## Desktop GUI Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Ctrl` + `=` | Zoom In Font Size |
| `Ctrl` + `-` | Zoom Out Font Size |
| `Ctrl` + `0` | Reset Zoom to Default |
| `Ctrl` + Mouse Wheel | Smooth Font Zoom In / Out |
| Titlebar `[A-]` / `[A+]` | Clickable zoom controls |

---

## Project Structure

```
RetroOS_Shell/
├── retroshell.c       # Core hybrid C shell engine (CMD aliases, scheduler, parser)
├── Makefile           # Compilation rules for C shell
├── main.js            # Electron main process (PTY bridge, global hotkey, window manager)
├── index.html         # Frontend GUI (xterm.js terminal canvas, custom titlebar)
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
| `retroshell: cannot switch to drive` | Ensure the drive is available in Windows (e.g. check if `E:\` is mounted) |
| Window closes instantly on launch | Run `wsl -e bash -c "ls -l ~/retroshell/retroshell"` to verify the binary is compiled |
| Powerline symbols show as boxes | Install a Nerd Font or launch using `./retroshell --ascii` |
| `Cannot find module 'electron'` | Run `npm install` inside the project root folder |

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
