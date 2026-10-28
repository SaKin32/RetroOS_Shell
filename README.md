# RetroOS Terminal & retroshell

> **Next-Generation Unix Shell & Desktop Terminal Environment**

RetroOS is a custom-crafted Unix/Linux shell (`retroshell`) bundled with a modern Electron & xterm.js desktop terminal interface featuring custom cyberpunk styling, Braille halftone art, live system analytics, and interactive OS process scheduling.

---

## Highlights & Features

- **Custom User Onboarding & Identity**:
  - Interactive Page 1 onboarding setup to configure your unique `@retros` identity.
  - Type `welcome` at any time to return to Page 1 or reconfigure.
  - Dynamically customized Powerline prompt: `[ username@retros ][ ~/dir ] >`.
- **Fastfetch Cyber Purple Cat Art**:
  - Precision 18-line Braille halftone cat rendering in Cyber Purple (`#c084fc`).
  - Real-time system stats tree (OS, Kernel, Uptime, Memory usage, CPU cores, color palette).
- **Desktop Terminal GUI (Electron + xterm.js)**:
  - Pitch black OLED background (`#000000`).
  - Minimalist custom title bar with Sky Cyan (`-`), Emerald Green (`[]`), and Rose Red (`X`) buttons.
  - **Dynamic Font Zoom**: Interactive `[A-] 15px [A+]` titlebar controls, `Ctrl + =` / `Ctrl + -` / `Ctrl + 0` keyboard shortcuts, and `Ctrl + MouseWheel` zoom.
- **Operating System Core Emulation**:
  - Pipelines (`cmd1 | cmd2 | cmd3`), background jobs (`&`), file redirections (`<`, `>`, `>>`).
  - Built-in CPU process scheduler: FCFS and Round-Robin Gantt charts (`sched fcfs`, `sched real`).
  - Live system process & resource monitor dashboard (`monitor`).
  - Ring privilege switcher (`mode-switch`: Ring 3 <-> Ring 0).
  - Multiple color themes (`theme retro`, `theme cyber`, `theme matrix`, `theme dracula`).

---

## Quick Start

### 1. Running the C Shell directly (Linux / WSL)

```bash
# Clone the repository
git clone https://github.com/<your-username>/retroos-terminal.git
cd retroos-terminal

# Compile and launch
make
./retroshell
```

### 2. Running the Desktop GUI (Electron)

Ensure Node.js is installed:

```bash
# Install dependencies
npm install

# Launch Electron Terminal
npm start
```

---

## Built-in Commands

| Command | Description |
| :--- | :--- |
| `welcome` | View Page 1 Welcome & Onboarding screen (press Enter to return) |
| `user [name]` | View or change your `@retros` username |
| `theme <name>` | Switch theme (`retro`, `cyber`, `matrix`, `dracula`) |
| `monitor` | Real-time CPU, RAM, and process activity dashboard |
| `sched` | Interactive OS scheduling simulator with Gantt charts |
| `mode-switch` | Toggle CPU privilege level (Ring 3 User <-> Ring 0 Kernel) |
| `help` | List all shell built-in commands |
| `clear` | Clear the screen |
| `exit` | Exit the shell |
