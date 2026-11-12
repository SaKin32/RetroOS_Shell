/*
 * RetroOS Shell  (retroshell)  --  Stage 3
 * ---------------------------------------------------------------
 * A real Unix shell: reads a line, parses it, then either runs a
 * built-in command or fork()s a child that exec()s the program.
 * Looks like the RetroOS-CLI HTML design (powerline prompt, themes).
 *
 * Build:  make            (needs Linux / WSL)
 * Run:    ./retroshell    (--ascii  if the arrow glyphs show as boxes)
 *
 * Stage 1 : prompt, tokenizer, fork/exec/wait, built-ins, sched fcfs   [done]
 * Stage 2 : I/O redirection  <  >  >>                                  [done]
 * Stage 3 : pipes  cmd1 | cmd2 | cmd3                                  [done]
 * Stage 4 : REAL scheduler (sched real ...)                            [done]
 * Stage 5 : live system monitor (monitor)                              [done]
 * Next    : trace, background jobs (&), history
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_ARGS   128
#define MAX_STAGES 16     /* max commands in one pipeline */
#define MAX_PROCS  10
#define BANNER_W   52   /* inner width of the welcome box */

/* ------------------------------------------------------------------
 * Colours (24-bit "truecolor" ANSI escape codes)
 * ------------------------------------------------------------------ */
typedef struct { int r, g, b; } RGB;

static void set_fg(RGB c) { printf("\x1b[38;2;%d;%d;%dm", c.r, c.g, c.b); }
static void set_bg(RGB c) { printf("\x1b[48;2;%d;%d;%dm", c.r, c.g, c.b); }
#define RESET "\x1b[0m"
#define BOLD  "\x1b[1m"

static const RGB DARK  = {0x02, 0x06, 0x17};
static const RGB WHITE = {0xff, 0xff, 0xff};
static const RGB RED   = {0xf8, 0x71, 0x71};
static const RGB GREEN = {0x34, 0xd3, 0x99};
static const RGB AMBER = {0xfb, 0xbf, 0x24};
static const RGB CYAN  = {0x22, 0xd3, 0xee};
static const RGB MUTED = {0x94, 0xa3, 0xb8};
static const RGB KERNEL_RED = {0xdc, 0x26, 0x26};

typedef struct {
    const char *name;
    RGB seg1, seg2, seg3, accent;
} Theme;

/* Same colours as the HTML themes */
static const Theme THEMES[] = {
    {"retro",   {0x93,0x33,0xea}, {0xec,0x48,0x99}, {0x22,0xd3,0xee}, {0x34,0xd3,0x99}},
    {"cyber",   {0xfa,0xcc,0x15}, {0x06,0xb6,0xd4}, {0xd9,0x46,0xef}, {0xfa,0xcc,0x15}},
    {"matrix",  {0x16,0xa3,0x4a}, {0x04,0x78,0x57}, {0x4a,0xde,0x80}, {0x4a,0xde,0x80}},
    {"dracula", {0xa8,0x55,0xf7}, {0xdb,0x27,0x77}, {0x81,0x8c,0xf8}, {0xbd,0x93,0xf9}},
};
#define NTHEMES ((int)(sizeof THEMES / sizeof THEMES[0]))

static const RGB GANTT[] = {
    {0x4f,0x46,0xe5}, {0x08,0x91,0xb2}, {0x93,0x33,0xea}, {0xdb,0x27,0x77},
    {0x05,0x96,0x69}, {0xd9,0x77,0x06}, {0x25,0x63,0xeb}, {0xdc,0x26,0x26},
};
#define NGANTT ((int)(sizeof GANTT / sizeof GANTT[0]))

/* ------------------------------------------------------------------
 * Shell state
 * ------------------------------------------------------------------ */
static int theme_idx   = 0;
static int kernel_mode = 0;   /* cosmetic only: simulated Ring 0 / Ring 3 */
static int last_status = 0;   /* exit status of the previous command      */
static int use_powerline = 1; /* 0 = plain ASCII separators               */

static volatile sig_atomic_t got_sigint = 0;   /* set by Ctrl+C */
static void sigint_handler(int sig) { (void)sig; got_sigint = 1; /* also interrupts getline() */ }

/* ------------------------------------------------------------------
 * Operating System Environments: Windows, Linux, Hybrid
 * ------------------------------------------------------------------ */
typedef enum {
    OS_MODE_HYBRID = 0,
    OS_MODE_WINDOWS = 1,
    OS_MODE_LINUX = 2
} OperatingMode;

static OperatingMode g_os_mode = OS_MODE_HYBRID;

static const char *get_dynamic_windows_home(char *buf, size_t sz)
{
    /* 1. Check Windows USERPROFILE environment variable if forwarded */
    const char *prof = getenv("USERPROFILE");
    if (prof && *prof) {
        if (isalpha((unsigned char)prof[0]) && prof[1] == ':') {
            char d = tolower((unsigned char)prof[0]);
            char converted[PATH_MAX];
            strncpy(converted, prof + 2, sizeof(converted) - 1);
            converted[sizeof(converted) - 1] = '\0';
            for (char *c = converted; *c; c++) if (*c == '\\') *c = '/';
            snprintf(buf, sz, "/mnt/%c%s", d, converted);
            if (access(buf, F_OK) == 0) return buf;
        }
    }

    /* 2. Execute cmd.exe to query the exact active Windows username without guessing */
    FILE *fp = popen("/mnt/c/Windows/System32/cmd.exe /c echo %USERNAME% 2>/dev/null", "r");
    if (fp) {
        char win_user[128];
        if (fgets(win_user, sizeof(win_user), fp)) {
            size_t l = strlen(win_user);
            while (l > 0 && (win_user[l - 1] == '\r' || win_user[l - 1] == '\n' || isspace((unsigned char)win_user[l - 1])))
                win_user[--l] = '\0';
            if (l > 0 && win_user[0] != '%') {
                snprintf(buf, sz, "/mnt/c/Users/%s", win_user);
                pclose(fp);
                if (access(buf, F_OK) == 0) return buf;
            }
        }
        pclose(fp);
    }

    /* 3. Fallback: Scan /mnt/c/Users/ for actual usernames excluding system folders */
    DIR *d = opendir("/mnt/c/Users");
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (strcasecmp(ent->d_name, "Public") == 0 ||
                strcasecmp(ent->d_name, "Default") == 0 ||
                strcasecmp(ent->d_name, "Default User") == 0 ||
                strcasecmp(ent->d_name, "All Users") == 0)
                continue;
            snprintf(buf, sz, "/mnt/c/Users/%s", ent->d_name);
            closedir(d);
            return buf;
        }
        closedir(d);
    }

    /* 4. If not running on WSL with /mnt/c, fallback to Linux HOME or cwd */
    const char *h = getenv("HOME");
    if (h && *h) return h;
    return ".";
}

static const char *get_os_mode_name(OperatingMode m)
{
    switch (m) {
        case OS_MODE_WINDOWS: return "Windows Native Mode";
        case OS_MODE_LINUX:   return "Linux Unix Mode";
        default:              return "Hybrid Interop Mode";
    }
}

static void select_os_boot_mode(void)
{
    if (!isatty(STDIN_FILENO)) return;

    printf("\x1b[2J\x1b[H\n");
    set_fg((RGB){0xcb, 0xa6, 0xf7});
    printf(BOLD "  ██████╗ ███████╗████████╗██████╗  ██████╗  ██████╗ ███████╗\n"
                "  ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██╔═══██╗██╔════╝\n"
                "  ██████╔╝█████╗     ██║   ██████╔╝██║   ██║██║   ██║███████╗\n"
                "  ██╔══██╗██╔══╝     ██║   ██╔══██╗██║   ██║██║   ██║╚════██║\n"
                "  ██║  ██║███████╗   ██║   ██║  ██║╚██████╔╝╚██████╔╝███████║\n"
                "  ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚══════╝\n" RESET);

    set_fg((RGB){0x38, 0xbd, 0xf8});
    printf(BOLD "\n  R E T R O   O S   B O O T   L O A D E R\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("  Select your preferred Operating System execution environment:\n");
    set_fg((RGB){0x58, 0x5b, 0x70});
    printf("  ─────────────────────────────────────────────────────────────────────────────\n");

    set_fg((RGB){0x38, 0xbd, 0xf8});
    printf(BOLD "  [1] Windows Engine Mode\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("      -> Default to C:\\ / E:\\, Windows CMD commands (dir, cls, ipconfig)\n\n");

    set_fg((RGB){0xa6, 0xe3, 0xa1});
    printf(BOLD "  [2] Linux Unix Engine Mode\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("      -> Default to ~/home, pure POSIX shell, schedulers, and Linux tools\n\n");

    set_fg((RGB){0xc0, 0x84, 0xfc});
    printf(BOLD "  [3] Hybrid Mode (Recommended)\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("      -> Seamless dual compatibility: run Windows apps + Linux OS together\n");

    set_fg((RGB){0x58, 0x5b, 0x70});
    printf("  ─────────────────────────────────────────────────────────────────────────────\n");
    set_fg((RGB){0xfa, 0xcc, 0x15});
    printf(BOLD "  Select Mode [1, 2, or 3] (Press Enter for Hybrid): " RESET);
    fflush(stdout);

    char win_path_buf[PATH_MAX];
    const char *detected_win_home = get_dynamic_windows_home(win_path_buf, sizeof(win_path_buf));

    char line[64];
    if (fgets(line, sizeof(line), stdin)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '1') {
            g_os_mode = OS_MODE_WINDOWS;
            chdir(detected_win_home);
        } else if (*p == '2') {
            g_os_mode = OS_MODE_LINUX;
            const char *home = getenv("HOME");
            if (home) chdir(home);
        } else {
            g_os_mode = OS_MODE_HYBRID;
            chdir(detected_win_home);
        }
    }
    /* Clear screen AND clear scrollback buffer completely (\x1b[3J) so previous boot menu never overlaps */
    printf("\x1b[H\x1b[2J\x1b[3J");
    fflush(stdout);
}

/* ------------------------------------------------------------------
 * User configuration & Prompt  [ user@retros ][ ~/dir ] ❯
 * ------------------------------------------------------------------ */
static char g_username[64] = "";

static void show_welcome_page(void)
{
    printf("\x1b[2J\x1b[H\n"); /* Clear screen for clean intro */
    set_fg((RGB){0xcb, 0xa6, 0xf7}); printf(BOLD "  ██████╗ ███████╗████████╗██████╗  ██████╗  ██████╗ ███████╗\n");
    set_fg((RGB){0xf3, 0x8b, 0xa8}); printf(BOLD "  ██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██╔═══██╗██╔════╝\n");
    set_fg((RGB){0xfa, 0xb3, 0x87}); printf(BOLD "  ██████╔╝█████╗     ██║   ██████╔╝██║   ██║██║   ██║███████╗\n");
    set_fg((RGB){0xa6, 0xe3, 0xa1}); printf(BOLD "  ██╔══██╗██╔══╝     ██║   ██╔══██╗██║   ██║██║   ██║╚════██║\n");
    set_fg((RGB){0x89, 0xdc, 0xeb}); printf(BOLD "  ██║  ██║███████╗   ██║   ██║  ██║╚██████╔╝╚██████╔╝███████║\n");
    set_fg((RGB){0x74, 0xc7, 0xec}); printf(BOLD "  ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚══════╝\n" RESET);

    set_fg((RGB){0xcb, 0xa6, 0xf7});
    printf(BOLD "\n  R E T R O   O S   T E R M I N A L   E N V I R O N M E N T\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("  Next-Generation Unix Shell Architecture | Version 1.3\n");
    set_fg((RGB){0x58, 0x5b, 0x70});
    printf("  ─────────────────────────────────────────────────────────────────────────────\n");
    set_fg((RGB){0xfa, 0xb3, 0x87});
    printf("  Welcome to RetroOS! Before launching your interactive session, please set\n"
           "  up your personal profile username.\n");
    set_fg((RGB){0x58, 0x5b, 0x70});
    printf("  ─────────────────────────────────────────────────────────────────────────────\n\n");
}

static void load_or_ask_username(void)
{
    const char *home = getenv("HOME");
    char path[PATH_MAX];
    if (home && *home) snprintf(path, sizeof(path), "%s/.retroshell_user", home);
    else strcpy(path, ".retroshell_user");

    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(g_username, sizeof(g_username), f)) {
            size_t l = strlen(g_username);
            while (l > 0 && (g_username[l - 1] == '\n' || g_username[l - 1] == '\r' || isspace((unsigned char)g_username[l - 1])))
                g_username[--l] = '\0';
        }
        fclose(f);
    }

    if (g_username[0] != '\0') return;

    /* First-time interactive setup for friend/user */
    if (isatty(STDIN_FILENO)) {
        show_welcome_page();

        set_fg((RGB){0xa6, 0xe3, 0xa1});
        printf(BOLD "  [?] Enter username: " RESET "\x1b[38;2;245;245;245m");
        fflush(stdout);

        char input[128];
        if (fgets(input, sizeof(input), stdin)) {
            char *p = input;
            while (isspace((unsigned char)*p)) p++;
            size_t l = strlen(p);
            while (l > 0 && (p[l - 1] == '\n' || p[l - 1] == '\r' || isspace((unsigned char)p[l - 1])))
                p[--l] = '\0';
            if (*p) {
                strncpy(g_username, p, sizeof(g_username) - 1);
                g_username[sizeof(g_username) - 1] = '\0';
            }
        }
        printf(RESET);

        /* New Page transition: clear screen AND scrollback buffer so user enters shell on a fresh page */
        printf("\x1b[H\x1b[2J\x1b[3J");
        fflush(stdout);
    }

    if (g_username[0] == '\0') {
        const char *u = getenv("USER");
        if (u && *u) strncpy(g_username, u, sizeof(g_username) - 1);
        else strcpy(g_username, "retro");
    }

    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", g_username);
        fclose(f);
    }
}

static void print_sep(RGB from, int has_next, RGB next)
{
    const char *glyph = use_powerline ? "\xee\x82\xb0" : ">";   /* U+E0B0 */
    set_fg(from);
    if (has_next) set_bg(next); else printf("\x1b[49m");
    printf("%s", glyph);
}

static void format_display_path(const char *raw_cwd, char *out, size_t out_sz)
{
    /* If path is under /mnt/<drive>/..., format it as Windows style C:\... */
    if (strncmp(raw_cwd, "/mnt/", 5) == 0 && isalpha((unsigned char)raw_cwd[5]) && (raw_cwd[6] == '/' || raw_cwd[6] == '\0')) {
        char drive = toupper((unsigned char)raw_cwd[5]);
        const char *rest = raw_cwd[6] == '/' ? raw_cwd + 7 : "";
        if (*rest == '\0') {
            snprintf(out, out_sz, "%c:\\", drive);
            return;
        }
        snprintf(out, out_sz, "%c:\\%s", drive, rest);
        /* Convert forward slashes to Windows backslashes */
        for (char *p = out + 3; *p; p++) {
            if (*p == '/') *p = '\\';
        }
        return;
    }

    const char *home = getenv("HOME");
    if (home && *home && strncmp(raw_cwd, home, strlen(home)) == 0 &&
        (raw_cwd[strlen(home)] == '/' || raw_cwd[strlen(home)] == '\0'))
        snprintf(out, out_sz, "~%s", raw_cwd + strlen(home));
    else
        snprintf(out, out_sz, "%s", raw_cwd);
}

static void print_prompt(void)
{
    const Theme *t = &THEMES[theme_idx];
    RGB c1 = kernel_mode ? KERNEL_RED : t->seg1;

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) strcpy(cwd, "?");
    char shown[PATH_MAX + 2];
    format_display_path(cwd, shown, sizeof shown);

    const char *uname = g_username[0] ? g_username : "retro";
    char user_tag[128];
    const char *mode_badge = g_os_mode == OS_MODE_WINDOWS ? "WIN" : (g_os_mode == OS_MODE_LINUX ? "LINUX" : "HYBRID");
    if (kernel_mode) {
        snprintf(user_tag, sizeof(user_tag), "root@retros [%s]", mode_badge);
    } else {
        snprintf(user_tag, sizeof(user_tag), "%s@retros [%s]", uname, mode_badge);
    }

    printf("\n");
    set_bg(c1);      set_fg(DARK); printf(BOLD " %s " RESET, user_tag);
    print_sep(c1, 1, t->seg2);
    set_bg(t->seg2); set_fg(DARK); printf(BOLD " %s " RESET, shown);
    print_sep(t->seg2, 0, t->seg2);
    printf(RESET "\n");

    set_fg(last_status == 0 ? t->accent : RED);
    printf(BOLD "%s " RESET, kernel_mode ? "#" : ">");
    fflush(stdout);
}

static void get_banner_stats(char *uptime_str, size_t up_sz, char *mem_str, size_t mem_sz, int *cores_out)
{
    int fd = open("/proc/uptime", O_RDONLY);
    double up = 0;
    if (fd >= 0) {
        char buf[64];
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (r > 0) { buf[r] = '\0'; sscanf(buf, "%lf", &up); }
    }
    long total_s = (long)up;
    long d = total_s / 86400, h = (total_s % 86400) / 3600, m = (total_s % 3600) / 60;
    if (d > 0) snprintf(uptime_str, up_sz, "%ldd %ldh %ldm", d, h, m);
    else if (h > 0) snprintf(uptime_str, up_sz, "%ldh %ldm", h, m);
    else snprintf(uptime_str, up_sz, "%ld mins", m > 0 ? m : 1);

    double mem_total = 0, mem_avail = 0;
    fd = open("/proc/meminfo", O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (r > 0) {
            buf[r] = '\0';
            char *p = strstr(buf, "MemTotal:");
            if (p) sscanf(p + 9, "%lf", &mem_total);
            p = strstr(buf, "MemAvailable:");
            if (p) sscanf(p + 13, "%lf", &mem_avail);
        }
    }
    double used_gib = (mem_total - mem_avail) / (1024.0 * 1024.0);
    double tot_gib = mem_total / (1024.0 * 1024.0);
    if (tot_gib > 0) snprintf(mem_str, mem_sz, "%.1f / %.1f GiB", used_gib, tot_gib);
    else snprintf(mem_str, mem_sz, "Normal");

    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    *cores_out = nprocs > 0 ? (int)nprocs : 1;
}

static const char *CAT_BRAILLE_LINES[] = {
    "   ⣀⣿⢷⣆⣀      ⢦⣤⢀            ⣀   ",
    "  ⢀⣿⣿⣿⣿⠾⣶⡀⠶⢾⣷⣧⣴⣿⣺⡟⣦⣤⡀    ⢀⣤⣼⣿⣿   ",
    "  ⢸⣿⣾⣾⣿⡐⣿⣿⣶⣄⠉⠟⣿⣿⣺⣷⣿⣿⣧⣀ ⣠⣾⣫⣿⣿⣿⣿   ",
    "  ⢠⣿⣿⣿⣿⡧⣿⢹⣿⣿⢿⣿⣼⣿⢸⣿⡷⣾⡜⢻⣿⣿⣿⠹⣷⣶⣿⡟   ",
    "  ⢘⣿⣿⣿⣿⠟⣿⣺⣿⣻⣟⣿⣛⣿⣺⣿⣿⣿⢺⣿⣿⣿⣿⢞⣿⣿⣿⡇   ",
    "  ⠈⣿⣿⣽⣽⣇⣿⣿⣿⣿⣿⣿⣭⣿⢹⣿⣿⣿⣩⣿⣿⣿⡟⢉⢹⣿⡿⠁   ",
    "   ⣛⣿⡿⠹⠋⠙⠙⢿⣻⣟⣿⣛⣯⣸⣿⣯⣿⣫⣽⣿⣿⣧⢁⣜⠻⠁    ",
    " ⡴⠼⣿⣟⣠⣼⡾⣿⣧⡄⣻⣿⣿⡿⣿⠚⠉⣁⣀⣄⡉⠹⣿⣿⡘⠛⢀⡀    ",
    "  ⢀⣿⡿⠻⣿⠟⣿⣿⣏⢉⣿⣿⣿⣿⢿⡿⣿⣿⣽⣿⣤⣿⣿⢿⡿⠿⠁    ",
    "  ⠰⠿⠿⠆⣠⢰⣺⢻⢿⢻⣿⣿⣿⠿⢸⣿⣟⠈⠈⠁⠛⠙⣻        ",
    "      ⠉⠘⠿⢾⣶⣌⣩⣉⣉⣀⢾⣿⣿⣾⣶⡶⠶⠛⠿⠰⠦      ",
    "        ⢀⡀⠈⣸⣿⣿⣯⣿⢻⣯⡥⢩⢄⣤           ",
    "         ⣸⣽⣿⣿⣿⠿⠝⢋⣁⣤⣤⢬⣧⡀          ",
    "        ⢠⢽⣿⣿⣿⠏⣀⣶⢼⡿⣿⣿⣽⣿⣷⣀         ",
    "          ⣿⣽⣿⣀⠿ ⣸⡇⠻⢿⠟⢿⣿⣿⣆        ",
    "          ⢸⣿⣟⣿⣤⣀⢈⡀ ⢠⣠⣟⣿⣟⣿⡄       ",
    "          ⢸⣿⣿⣿⣿⢿⣻⡿⡇⠸⣿⣿⣿⣿⣿⡄       ",
    "          ⢸⣿⣿⣿⣿⡄⢹⣿⣿⡄⢻⣿⣿⠟⠋        "
};
#define NCAT_LINES ((int)(sizeof(CAT_BRAILLE_LINES)/sizeof(CAT_BRAILLE_LINES[0])))

static void print_banner(void)
{
    const Theme *t = &THEMES[theme_idx];
    const char *u = g_username[0] ? g_username : "retro";

    const RGB CAT_PURP  = {0xc0, 0x84, 0xfc}; /* Vibrant Cyber Purple matching Image */
    const RGB STAT_SYS  = {0xf4, 0x72, 0xb6}; /* Coral for System */
    const RGB STAT_DESK = {0xfb, 0xbf, 0x24}; /* Amber for Desktop */
    const RGB STAT_HW   = {0x38, 0xbd, 0xf8}; /* Sky Blue for Hardware */
    const RGB STAT_PAL  = {0x4a, 0xde, 0x80}; /* Mint Green for Palette */
    const RGB DIV_COL   = {0x58, 0x5b, 0x70}; /* Divider line */

    char up_str[64], mem_str[64];
    int ncpu = 1;
    get_banner_stats(up_str, sizeof(up_str), mem_str, sizeof(mem_str), &ncpu);

    /* Dynamically read Kernel, Hostname and local IP */
    char kernel_ver[256] = "Linux";
    struct utsname u_info;
    if (uname(&u_info) == 0) {
        snprintf(kernel_ver, sizeof(kernel_ver), "%s %s", u_info.sysname, u_info.release);
    }

    char host_name[128] = "RetroTerminal";
    if (gethostname(host_name, sizeof(host_name)) != 0) {
        strncpy(host_name, "RetroHost", sizeof(host_name) - 1);
    }

    char local_ip[64] = "127.0.0.1";
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct sockaddr_in loopback;
        memset(&loopback, 0, sizeof(loopback));
        loopback.sin_family = AF_INET;
        loopback.sin_addr.s_addr = inet_addr("8.8.8.8");
        loopback.sin_port = htons(53);
        if (connect(sock, (struct sockaddr *)&loopback, sizeof(loopback)) == 0) {
            struct sockaddr_in name;
            socklen_t namelen = sizeof(name);
            if (getsockname(sock, (struct sockaddr *)&name, &namelen) == 0) {
                const char *p = inet_ntop(AF_INET, &name.sin_addr, local_ip, sizeof(local_ip));
                if (!p) strncpy(local_ip, "127.0.0.1", sizeof(local_ip) - 1);
            }
        }
        close(sock);
    }

    printf("\n");
    /* Side-by-side: Exact Halftone Braille Purple Cat (Left) + System stats tree (Right) */
    /* Line 0 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[0]);
    set_fg(STAT_PAL); printf("%s@retros\n", u);

    /* Line 1 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[1]);
    set_fg(MUTED);    printf("----------------------------------------\n");

    /* Line 2 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[2]);
    set_fg(STAT_SYS); printf("o SYSTEM\n");

    /* Line 3 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[3]);
    set_fg(STAT_SYS); printf("  |-- OS      -> ");
    set_fg(WHITE);    printf("RetroOS (Unix/Linux x86_64)\n");

    /* Line 4 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[4]);
    set_fg(STAT_SYS); printf("  |-- Kernel  -> ");
    set_fg(WHITE);    printf("%s\n", kernel_ver);

    /* Line 5 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[5]);
    set_fg(STAT_SYS); printf("  |-- Uptime  -> ");
    set_fg(WHITE);    printf("%s\n", up_str);

    /* Line 6 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[6]);
    set_fg(STAT_SYS); printf("  |-- Shell   -> ");
    set_fg(WHITE);    printf("retroshell v1.3 (Interactive)\n");

    /* Line 7 */
    set_fg(CAT_PURP); printf("%s  \n", CAT_BRAILLE_LINES[7]);

    /* Line 8 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[8]);
    set_fg(STAT_DESK); printf("o DESKTOP\n");

    /* Line 9 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[9]);
    set_fg(STAT_DESK); printf("  |-- DE      -> ");
    set_fg(WHITE);     printf("RetroOS Workspace GUI\n");

    /* Line 10 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[10]);
    set_fg(STAT_DESK); printf("  |-- WM      -> ");
    set_fg(WHITE);     printf("Electron PTY + xterm.js\n");

    /* Line 11 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[11]);
    set_fg(STAT_DESK); printf("  |-- Theme   -> ");
    set_fg(WHITE);     printf("Cyber Purple [RetroOS]\n");

    /* Line 12 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[12]);
    set_fg(STAT_DESK); printf("  |-- Font    -> ");
    set_fg(WHITE);     printf("RetroNerd Mono (15pt)\n");

    /* Line 13 */
    set_fg(CAT_PURP); printf("%s  \n", CAT_BRAILLE_LINES[13]);

    /* Line 14 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[14]);
    set_fg(STAT_HW);  printf("o HARDWARE\n");

    /* Line 15 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[15]);
    set_fg(STAT_HW);  printf("  |-- Host    -> ");
    set_fg(WHITE);    printf("%s\n", host_name);

    /* Line 16 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[16]);
    set_fg(STAT_HW);  printf("  |-- CPU     -> ");
    set_fg(WHITE);    printf("%d Cores Online\n", ncpu);

    /* Line 17 */
    set_fg(CAT_PURP); printf("%s  ", CAT_BRAILLE_LINES[17]);
    set_fg(STAT_HW);  printf("  |-- Memory  -> ");
    set_fg(WHITE);    printf("%s\n", mem_str);

    /* Line 18 */
    printf("                                           ");
    set_fg(STAT_HW);  printf("  |-- IP      -> ");
    set_fg(WHITE);    printf("%s\n", local_ip);

    /* Line 19 */
    printf("                                           \n");

    /* Line 20 */
    printf("                                           ");
    set_fg(STAT_PAL); printf("o PALETTE\n");

    /* Line 21 */
    printf("                                           ");
    set_fg((RGB){0xff, 0xff, 0xff}); printf("  (*) ");
    set_fg((RGB){0xa6, 0xe3, 0xa1}); printf("(*) ");
    set_fg((RGB){0x89, 0xdc, 0xeb}); printf("(*) ");
    set_fg((RGB){0xcb, 0xa6, 0xf7}); printf("(*) ");
    set_fg((RGB){0x89, 0xb4, 0xfa}); printf("(*) ");
    set_fg((RGB){0xfa, 0xb3, 0x87}); printf("(*) ");
    set_fg((RGB){0xf3, 0x8b, 0xa8}); printf("(*)\n" RESET);

    /* Spaced welcome greeting for user */
    char spaced[128];
    int sp_len = 0;
    for (int i = 0; u[i] && sp_len < (int)sizeof(spaced) - 3; i++) {
        spaced[sp_len++] = toupper((unsigned char)u[i]);
        spaced[sp_len++] = ' ';
    }
    if (sp_len > 0 && spaced[sp_len - 1] == ' ') sp_len--;
    spaced[sp_len] = '\0';

    set_fg(DIV_COL);
    printf("  ─────────────────────────────────────────────────────────────────────────────\n");
    set_fg(CAT_PURP);
    printf(BOLD "  W E L C O M E   B A C K ,   %s\n" RESET, spaced);
    set_fg((RGB){0xfa, 0xb3, 0x87});
    printf("  You are not just a user behind a terminal. You are PID 1.\n");
    set_fg(CAT_PURP);
    printf(BOLD "  The shell is not a tool. It is your will, made executable.\n" RESET);
    set_fg(DIV_COL);
    printf("  ─────────────────────────────────────────────────────────────────────────────\n");

    set_fg((RGB){0xec, 0xdf, 0xfc});
    printf("  Where did you want to go, or what did you want to do?\n");
    set_fg(MUTED);
    printf("  (Type ");
    set_fg(AMBER); printf("help"); set_fg(MUTED);
    printf(" for commands | ");
    set_fg(CYAN);  printf("user <name>"); set_fg(MUTED);
    printf(" to rename | ");
    set_fg(t->accent); printf("theme <name>"); set_fg(MUTED);
    printf(" to restyle)\n" RESET);
}

/* ------------------------------------------------------------------
 * Stage 2/3 data: one pipeline = up to MAX_STAGES commands.
 * Each command has its argv and optional < > >> redirections.
 * ------------------------------------------------------------------ */
typedef struct {
    char *argv[MAX_ARGS];
    int   argc;
    char *in_file;     /* < file   (NULL = none)                 */
    char *out_file;    /* > file or >> file (NULL = none)        */
    int   append;      /* 1 for >>, 0 for >                      */
    char *err_file;    /* 2> or 2>> file (NULL = none)           */
    int   err_append;  /* 1 for 2>>, 0 for 2>                    */
    int   err_to_out;  /* 1 for 2>&1                             */
    int   out_to_err;  /* 1 for 1>&2                             */
} Stage;

/* ------------------------------------------------------------------
 * Tokenizer: splits a line into words AND operators ( | < > >> ).
 * Quotes ('...' or "...") protect spaces and operators, e.g.
 *     echo "a | b"   -> the | is just text, not a pipe.
 * Output strings are written into `buf` (needs 2*strlen(line)+2
 * bytes). isop[i] is 1 when tok[i] is a real (unquoted) operator.
 * Returns the token count, or -1 on an error.
 * ------------------------------------------------------------------ */
static int is_opchar(char c) { return c == '|' || c == '<' || c == '>' || c == ';' || c == '&'; }

/* ------------------------------------------------------------------
 * Arithmetic Expression Evaluator for $((expr))
 * Supports +, -, *, /, %, parentheses, bitwise (&, |, ^), shifts (<<, >>),
 * comparison, overflow checks, and variable lookup.
 * ------------------------------------------------------------------ */
typedef struct {
    int has_error;
    char error_msg[128];
} ArithContext;

static long long eval_expr_bitwise_or(const char **s, ArithContext *ctx);

static void arith_error(ArithContext *ctx, const char *msg)
{
    if (!ctx->has_error) {
        ctx->has_error = 1;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), "%s", msg);
    }
}

static long long eval_expr_primary(const char **s, ArithContext *ctx)
{
    while (isspace((unsigned char)**s)) (*s)++;
    if (**s == '\0') {
        arith_error(ctx, "syntax error: operand expected");
        return 0;
    }
    if (**s == '(') {
        (*s)++;
        long long val = eval_expr_bitwise_or(s, ctx);
        while (isspace((unsigned char)**s)) (*s)++;
        if (**s == ')') {
            (*s)++;
        } else {
            arith_error(ctx, "syntax error: missing ')'");
        }
        return val;
    }
    if (isdigit((unsigned char)**s)) {
        char *endptr;
        errno = 0;
        long long val = strtoll(*s, &endptr, 0);
        *s = endptr;
        return val;
    }
    if (isalpha((unsigned char)**s) || **s == '_') {
        const char *start = *s;
        while (isalnum((unsigned char)**s) || **s == '_') (*s)++;
        char var_name[128];
        size_t len = (size_t)(*s - start);
        if (len >= sizeof(var_name)) len = sizeof(var_name) - 1;
        memcpy(var_name, start, len);
        var_name[len] = '\0';
        const char *v = getenv(var_name);
        if (v && *v) {
            char *endptr;
            return strtoll(v, &endptr, 0);
        }
        return 0;
    }
    arith_error(ctx, "syntax error: unexpected token");
    return 0;
}

static long long eval_expr_unary(const char **s, ArithContext *ctx)
{
    while (isspace((unsigned char)**s)) (*s)++;
    if (**s == '+') { (*s)++; return eval_expr_unary(s, ctx); }
    if (**s == '-') {
        (*s)++;
        long long v = eval_expr_unary(s, ctx);
        if (v == LLONG_MIN) return LLONG_MIN; /* wrap on overflow */
        return -v;
    }
    if (**s == '~') {
        (*s)++;
        return ~eval_expr_unary(s, ctx);
    }
    if (**s == '!') {
        (*s)++;
        return !eval_expr_unary(s, ctx);
    }
    return eval_expr_primary(s, ctx);
}

static long long eval_expr_mul(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_unary(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        char op = **s;
        if (op == '*' || op == '/' || op == '%') {
            (*s)++;
            long long right = eval_expr_unary(s, ctx);
            if (ctx->has_error) return 0;
            if (op == '*') {
                long long res;
                if (__builtin_mul_overflow(val, right, &res)) {
                    /* On signed overflow, perform modular wrap-around */
                    val = (long long)((unsigned long long)val * (unsigned long long)right);
                } else {
                    val = res;
                }
            } else if (op == '/') {
                if (right == 0) {
                    arith_error(ctx, "division by 0");
                    return 0;
                }
                if (val == LLONG_MIN && right == -1) {
                    val = LLONG_MIN; /* Prevent SIGFPE on x86 LLONG_MIN / -1 */
                } else {
                    val = val / right;
                }
            } else if (op == '%') {
                if (right == 0) {
                    arith_error(ctx, "division by 0");
                    return 0;
                }
                if (val == LLONG_MIN && right == -1) {
                    val = 0; /* LLONG_MIN % -1 is mathematically 0 */
                } else {
                    val = val % right;
                }
            }
        } else break;
    }
    return val;
}

static long long eval_expr_add(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_mul(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        char op = **s;
        if (op == '+' || op == '-') {
            (*s)++;
            long long right = eval_expr_mul(s, ctx);
            if (ctx->has_error) return 0;
            if (op == '+') {
                long long res;
                if (__builtin_add_overflow(val, right, &res)) {
                    val = (long long)((unsigned long long)val + (unsigned long long)right);
                } else {
                    val = res;
                }
            } else {
                long long res;
                if (__builtin_sub_overflow(val, right, &res)) {
                    val = (long long)((unsigned long long)val - (unsigned long long)right);
                } else {
                    val = res;
                }
            }
        } else break;
    }
    return val;
}

static long long eval_expr_shift(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_add(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        if ((*s)[0] == '<' && (*s)[1] == '<') {
            *s += 2;
            long long right = eval_expr_add(s, ctx);
            if (right < 0 || right >= 64) val = 0;
            else val = (long long)((unsigned long long)val << right);
        } else if ((*s)[0] == '>' && (*s)[1] == '>') {
            *s += 2;
            long long right = eval_expr_add(s, ctx);
            if (right < 0) val = 0;
            else if (right >= 64) val = (val < 0) ? -1 : 0;
            else val = val >> right;
        } else break;
    }
    return val;
}

static long long eval_expr_cmp(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_shift(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        if ((*s)[0] == '<' && (*s)[1] == '=') {
            *s += 2;
            long long right = eval_expr_shift(s, ctx);
            val = (val <= right);
        } else if ((*s)[0] == '>' && (*s)[1] == '=') {
            *s += 2;
            long long right = eval_expr_shift(s, ctx);
            val = (val >= right);
        } else if ((*s)[0] == '<' && (*s)[1] != '<') {
            (*s)++;
            long long right = eval_expr_shift(s, ctx);
            val = (val < right);
        } else if ((*s)[0] == '>' && (*s)[1] != '>') {
            (*s)++;
            long long right = eval_expr_shift(s, ctx);
            val = (val > right);
        } else break;
    }
    return val;
}

static long long eval_expr_eq(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_cmp(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        if ((*s)[0] == '=' && (*s)[1] == '=') {
            *s += 2;
            long long right = eval_expr_cmp(s, ctx);
            val = (val == right);
        } else if ((*s)[0] == '!' && (*s)[1] == '=') {
            *s += 2;
            long long right = eval_expr_cmp(s, ctx);
            val = (val != right);
        } else break;
    }
    return val;
}

static long long eval_expr_bitwise_and(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_eq(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        if (**s == '&' && (*s)[1] != '&') {
            (*s)++;
            long long right = eval_expr_eq(s, ctx);
            val &= right;
        } else break;
    }
    return val;
}

static long long eval_expr_bitwise_xor(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_bitwise_and(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        if (**s == '^') {
            (*s)++;
            long long right = eval_expr_bitwise_and(s, ctx);
            val ^= right;
        } else break;
    }
    return val;
}

static long long eval_expr_bitwise_or(const char **s, ArithContext *ctx)
{
    long long val = eval_expr_bitwise_xor(s, ctx);
    for (;;) {
        while (isspace((unsigned char)**s)) (*s)++;
        if (**s == '|' && (*s)[1] != '|') {
            (*s)++;
            long long right = eval_expr_bitwise_xor(s, ctx);
            val |= right;
        } else break;
    }
    return val;
}

static int eval_arithmetic_str(const char *expr, long long *result, char *err_buf, size_t err_sz)
{
    ArithContext ctx = {0, ""};
    const char *p = expr;
    long long res = eval_expr_bitwise_or(&p, &ctx);
    while (isspace((unsigned char)*p)) p++;
    if (!ctx.has_error && *p != '\0') {
        arith_error(&ctx, "syntax error: unexpected characters at end of expression");
    }
    if (ctx.has_error) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "%s", ctx.error_msg);
        }
        return -1;
    }
    *result = res;
    return 0;
}

/* Expands $VAR, $?, $$, ${VAR}, or $((expr)) at *p_ptr and writes result into *out_ptr */
static void expand_var(const char **p_ptr, char **out_ptr, char *buf, size_t buf_sz)
{
    const char *p = *p_ptr;
    char *out = *out_ptr;

    p++; /* skip '$' */

    /* Arithmetic expansion: $((expression)) */
    if (*p == '(' && p[1] == '(') {
        p += 2;
        const char *start = p;
        int depth = 1;
        while (*p) {
            if (*p == '(' && p[1] == '(') { depth++; p += 2; continue; }
            if (*p == ')' && p[1] == ')') {
                depth--;
                if (depth == 0) break;
                p += 2;
                continue;
            }
            p++;
        }
        size_t elen = (size_t)(p - start);
        char expr[256];
        if (elen >= sizeof(expr)) elen = sizeof(expr) - 1;
        memcpy(expr, start, elen);
        expr[elen] = '\0';
        if (*p == ')' && p[1] == ')') p += 2;

        long long res = 0;
        char err_msg[128] = {0};
        if (eval_arithmetic_str(expr, &res, err_msg, sizeof(err_msg)) < 0) {
            fprintf(stderr, "retroshell: %s: %s\n", expr, err_msg);
            last_status = 1;
        } else {
            char tmp[64];
            int l = snprintf(tmp, sizeof tmp, "%lld", res);
            for (int i = 0; i < l && (size_t)(out - buf) < buf_sz - 1; i++)
                *out++ = tmp[i];
        }

        *p_ptr = p;
        *out_ptr = out;
        return;
    }

    if (*p == '?') {
        p++;
        char tmp[32];
        int l = snprintf(tmp, sizeof tmp, "%d", last_status);
        for (int i = 0; i < l && (size_t)(out - buf) < buf_sz - 1; i++)
            *out++ = tmp[i];
    } else if (*p == '$') {
        p++;
        char tmp[32];
        int l = snprintf(tmp, sizeof tmp, "%d", (int)getpid());
        for (int i = 0; i < l && (size_t)(out - buf) < buf_sz - 1; i++)
            *out++ = tmp[i];
    } else if (*p == '{') {
        p++; /* skip '{' */
        const char *start = p;
        while (*p && *p != '}') p++;
        size_t vlen = (size_t)(p - start);
        if (*p == '}') p++; /* skip '}' */

        if (vlen > 0 && vlen < 256) {
            char name[256];
            memcpy(name, start, vlen);
            name[vlen] = '\0';
            const char *val = getenv(name);
            if (val) {
                while (*val && (size_t)(out - buf) < buf_sz - 1)
                    *out++ = *val++;
            }
        }
    } else if (isalpha((unsigned char)*p) || *p == '_') {
        const char *start = p;
        while (isalnum((unsigned char)*p) || *p == '_') p++;
        size_t vlen = (size_t)(p - start);
        if (vlen > 0 && vlen < 256) {
            char name[256];
            memcpy(name, start, vlen);
            name[vlen] = '\0';
            const char *val = getenv(name);
            if (val) {
                while (*val && (size_t)(out - buf) < buf_sz - 1)
                    *out++ = *val++;
            }
        }
    } else {
        /* Lone '$' character (e.g. at end of line or before whitespace) */
        if ((size_t)(out - buf) < buf_sz - 1)
            *out++ = '$';
    }

    *p_ptr = p;
    *out_ptr = out;
}

/* ------------------------------------------------------------------
 * Memory pool for dynamically expanded glob tokens
 * ------------------------------------------------------------------ */
static char *glob_pool[MAX_ARGS * 8];
static int glob_pool_count = 0;

static char *pool_strdup(const char *s)
{
    char *copy = strdup(s);
    if (copy && glob_pool_count < (int)(sizeof(glob_pool) / sizeof(glob_pool[0]))) {
        glob_pool[glob_pool_count++] = copy;
    }
    return copy;
}

static void pool_free_all(void)
{
    for (int i = 0; i < glob_pool_count; i++) {
        free(glob_pool[i]);
    }
    glob_pool_count = 0;
}

static int tokenize(const char *line, char *buf, size_t buf_sz, char **tok, int *isop, int *isglob, int max)
{
    int n = 0;
    const char *p = line;
    char *out = buf;

    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (n >= max - 1) { fprintf(stderr, "retroshell: too many arguments\n"); return -1; }

        tok[n] = out;
        if ((*p == '2' || *p == '1') && p[1] == '>') {
            isop[n] = 1;
            if (isglob) isglob[n] = 0;
            n++;
            char fd_num = *p++;
            *out++ = fd_num;
            *out++ = *p++; /* '>' */
            if (fd_num == '2' && *p == '&' && p[1] == '1') {
                *out++ = *p++; *out++ = *p++; /* 2>&1 */
            } else if (fd_num == '1' && *p == '&' && p[1] == '2') {
                *out++ = *p++; *out++ = *p++; /* 1>&2 */
            } else if (*p == '>') {
                *out++ = *p++; /* 2>> or 1>> */
            }
            *out++ = '\0';
            continue;
        }

        if (is_opchar(*p)) {                         /* operator token */
            isop[n] = 1;
            if (isglob) isglob[n] = 0;
            n++;
            if (*p == ';' || *p == '<') {
                *out++ = *p++;
            } else if (*p == '>') {
                *out++ = *p++;
                if (*p == '>') *out++ = *p++;   /* >> */
            } else if (*p == '&') {
                *out++ = *p++;
                if (*p == '&') {
                    *out++ = *p++;              /* && */
                } else if (*p == '>') {
                    *out++ = *p++;              /* &> */
                    if (*p == '>') *out++ = *p++; /* &>> */
                }
            } else if (*p == '|') {
                *out++ = *p++;
                if (*p == '|') *out++ = *p++;   /* || */
            }
            *out++ = '\0';
            continue;
        }

        isop[n] = 0;                                 /* word token */
        if (isglob) isglob[n] = 0;
        char quote = 0;
        int quoted = 0;
        while (*p) {
            if (quote) {
                if (*p == quote) {
                    quote = 0;
                    p++;
                } else if (quote == '"' && *p == '\\' && (p[1] == '$' || p[1] == '"' || p[1] == '\\')) {
                    p++;
                    if ((size_t)(out - buf) < buf_sz - 1) *out++ = *p++;
                } else if (quote == '"' && *p == '$') {
                    expand_var(&p, &out, buf, buf_sz);
                } else {
                    if ((size_t)(out - buf) < buf_sz - 1) *out++ = *p++;
                }
            } else if (*p == '"' || *p == '\'') {
                quote = *p++;
                quoted = 1;
            } else if (*p == '$' && p[1] == '(' && p[2] == '(') {
                expand_var(&p, &out, buf, buf_sz);
            } else if (isspace((unsigned char)*p) || is_opchar(*p) || ((*p == '1' || *p == '2') && p[1] == '>')) {
                break;                               /* word ends here */
            } else if (*p == '\\' && p[1] != '\0') {
                p++;
                if ((size_t)(out - buf) < buf_sz - 1) *out++ = *p++;
            } else if (*p == '~' && out == tok[n] && (p[1] == '/' || p[1] == '\0' || isspace((unsigned char)p[1]) || is_opchar(p[1]))) {
                /* Tilde expansion at start of unquoted word */
                p++;
                const char *home = getenv("HOME");
                if (home) {
                    while (*home && (size_t)(out - buf) < buf_sz - 1)
                        *out++ = *home++;
                }
            } else if (*p == '$') {
                expand_var(&p, &out, buf, buf_sz);
            } else {
                if (*p == '*' || *p == '?' || *p == '[') {
                    if (isglob) isglob[n] = 1;
                }
                if ((size_t)(out - buf) < buf_sz - 1) *out++ = *p++;
            }
        }
        if (quote) { fprintf(stderr, "retroshell: unterminated quote\n"); return -1; }

        /* If unquoted token produced zero characters (e.g. $UNDEFINED), omit it */
        if (!quoted && out == tok[n]) {
            continue;
        }

        *out++ = '\0';
        n++;
    }
    return n;
}

/* Turn tokens into pipeline stages with wildcard globbing expansion. Returns 0 on success, -1 on syntax error. */
static int build_pipeline(char **tok, int *isop, int *isglob, int n, Stage *st, int *nst)
{
    int s = 0;
    memset(&st[0], 0, sizeof st[0]);

    for (int i = 0; i < n; i++) {
        if (!isop[i]) {
            if (isglob && isglob[i]) {
                glob_t g;
                memset(&g, 0, sizeof(g));
                int gr = glob(tok[i], GLOB_NOCHECK, NULL, &g);
                if (gr == 0) {
                    for (size_t k = 0; k < g.gl_pathc; k++) {
                        if (st[s].argc >= MAX_ARGS - 1) break;
                        st[s].argv[st[s].argc++] = pool_strdup(g.gl_pathv[k]);
                    }
                    globfree(&g);
                    continue;
                }
            }
            if (st[s].argc >= MAX_ARGS - 1) { fprintf(stderr, "retroshell: too many arguments\n"); return -1; }
            st[s].argv[st[s].argc++] = tok[i];
            continue;
        }
        const char *op = tok[i];
        if (strcmp(op, "|") == 0) {
            if (st[s].argc == 0) { fprintf(stderr, "retroshell: syntax error near '|'\n"); return -1; }
            if (s + 1 >= MAX_STAGES) { fprintf(stderr, "retroshell: too many commands in a pipeline (max %d)\n", MAX_STAGES); return -1; }
            s++;
            memset(&st[s], 0, sizeof st[s]);
        } else if (strcmp(op, "2>&1") == 0) {
            st[s].err_to_out = 1;
        } else if (strcmp(op, "1>&2") == 0) {
            st[s].out_to_err = 1;
        } else {
            if (i + 1 >= n || isop[i + 1]) {
                fprintf(stderr, "retroshell: syntax error: expected a file name after '%s'\n", op);
                return -1;
            }
            char *file = tok[++i];
            if (isglob && isglob[i]) {
                glob_t g;
                memset(&g, 0, sizeof(g));
                if (glob(file, 0, NULL, &g) == 0 && g.gl_pathc == 1) {
                    file = pool_strdup(g.gl_pathv[0]);
                }
                globfree(&g);
            }
            if (strcmp(op, "<") == 0) {
                st[s].in_file = file;
            } else if (strcmp(op, ">") == 0 || strcmp(op, "1>") == 0) {
                st[s].out_file = file;
                st[s].append = 0;
            } else if (strcmp(op, ">>") == 0 || strcmp(op, "1>>") == 0) {
                st[s].out_file = file;
                st[s].append = 1;
            } else if (strcmp(op, "2>") == 0) {
                st[s].err_file = file;
                st[s].err_append = 0;
            } else if (strcmp(op, "2>>") == 0) {
                st[s].err_file = file;
                st[s].err_append = 1;
            } else if (strcmp(op, "&>") == 0) {
                st[s].out_file = file;
                st[s].append = 0;
                st[s].err_file = file;
                st[s].err_append = 0;
            } else if (strcmp(op, "&>>") == 0) {
                st[s].out_file = file;
                st[s].append = 1;
                st[s].err_file = file;
                st[s].err_append = 1;
            }
        }
    }
    if (st[s].argc == 0) { fprintf(stderr, "retroshell: syntax error: missing command\n"); return -1; }
    *nst = s + 1;
    return 0;
}

#define MAX_COMMANDS 32

typedef enum {
    OP_NONE = 0,
    OP_SEQ,      /* ; */
    OP_AND,      /* && */
    OP_OR,       /* || */
    OP_BG        /* & */
} ChainOp;

typedef struct {
    char *cmd_str;
    ChainOp next_op;
    int is_bg;
} LineSegment;

/* Split input line into command segments separated by ;, &&, ||, & */
static int split_line_segments(const char *line, LineSegment *segs, int max_segs)
{
    int count = 0;
    const char *start = line;
    const char *p = line;
    char quote = 0;

    while (*p) {
        if (quote) {
            if (*p == quote) quote = 0;
            else if (quote == '"' && *p == '\\' && p[1] != '\0') p++;
            p++;
            continue;
        }

        if (*p == '\'' || *p == '"') {
            quote = *p++;
            continue;
        }

        /* Skip $(( ... )) so operators like | and & inside arithmetic expressions are not treated as chain/job operators */
        if (*p == '$' && p[1] == '(' && p[2] == '(') {
            p += 3;
            int depth = 1;
            while (*p) {
                if (*p == '(' && p[1] == '(') { depth++; p += 2; continue; }
                if (*p == ')' && p[1] == ')') {
                    depth--;
                    if (depth == 0) { p += 2; break; }
                    p += 2;
                    continue;
                }
                p++;
            }
            continue;
        }

        ChainOp op = OP_NONE;
        int op_len = 0;

        if (*p == ';') {
            op = OP_SEQ; op_len = 1;
        } else if (*p == '&') {
            if (p[1] == '&') { op = OP_AND; op_len = 2; }
            else if (p[1] != '>') { op = OP_BG; op_len = 1; }
        } else if (*p == '|' && p[1] == '|') {
            op = OP_OR; op_len = 2;
        }

        if (op != OP_NONE) {
            if (count >= max_segs) break;
            size_t seg_len = (size_t)(p - start);
            char *sub = malloc(seg_len + 1);
            if (!sub) return -1;
            memcpy(sub, start, seg_len);
            sub[seg_len] = '\0';

            segs[count].cmd_str = sub;
            segs[count].next_op = op;
            segs[count].is_bg = (op == OP_BG);
            count++;

            p += op_len;
            start = p;
            continue;
        }
        p++;
    }

    if (start < p && count < max_segs) {
        size_t seg_len = (size_t)(p - start);
        char *sub = malloc(seg_len + 1);
        if (!sub) return -1;
        memcpy(sub, start, seg_len);
        sub[seg_len] = '\0';

        segs[count].cmd_str = sub;
        segs[count].next_op = OP_NONE;
        segs[count].is_bg = 0;
        count++;
    }
    return count;
}

/* Open the < and > files and plug them into stdin / stdout. */
static int apply_redirs(const Stage *s)
{
    if (s->in_file) {
        int fd = open(s->in_file, O_RDONLY);
        if (fd < 0) { fprintf(stderr, "retroshell: %s: %s\n", s->in_file, strerror(errno)); return -1; }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (s->out_file) {
        int flags = O_WRONLY | O_CREAT | (s->append ? O_APPEND : O_TRUNC);
        int fd = open(s->out_file, flags, 0644);
        if (fd < 0) { fprintf(stderr, "retroshell: %s: %s\n", s->out_file, strerror(errno)); return -1; }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    if (s->err_file) {
        int flags = O_WRONLY | O_CREAT | (s->err_append ? O_APPEND : O_TRUNC);
        int fd = open(s->err_file, flags, 0644);
        if (fd < 0) { fprintf(stderr, "retroshell: %s: %s\n", s->err_file, strerror(errno)); return -1; }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
    if (s->err_to_out) {
        dup2(STDOUT_FILENO, STDERR_FILENO);
    }
    if (s->out_to_err) {
        dup2(STDERR_FILENO, STDOUT_FILENO);
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Built-in commands (must run inside the shell process itself:
 * e.g. `cd` in a child would only change the child's directory)
 * ------------------------------------------------------------------ */
static int bi_cd(int argc, char **argv)
{
    const char *target = argv[1];
    char old[PATH_MAX];
    if (!getcwd(old, sizeof old)) old[0] = '\0';

    if (argc > 2) { fprintf(stderr, "cd: too many arguments\n"); return 1; }
    if (!target || strcmp(target, "~") == 0) target = getenv("HOME");
    else if (strcmp(target, "-") == 0) {
        target = getenv("OLDPWD");
        if (!target) { fprintf(stderr, "cd: OLDPWD not set\n"); return 1; }
        printf("%s\n", target);
    }
    if (!target) { fprintf(stderr, "cd: HOME not set\n"); return 1; }

    /* Normalize Windows paths: e.g. C:\Users\... or path\subpath or cd \ */
    char resolved[PATH_MAX];
    strncpy(resolved, target, sizeof(resolved) - 1);
    resolved[sizeof(resolved) - 1] = '\0';

    /* Convert backslashes to forward slashes */
    for (char *p = resolved; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    /* If user types 'C:/Users/...' or 'c:/...' map to '/mnt/c/...' */
    if (isalpha((unsigned char)resolved[0]) && resolved[1] == ':') {
        char drive = tolower((unsigned char)resolved[0]);
        char win_trans[PATH_MAX];
        if (resolved[2] == '/') {
            snprintf(win_trans, sizeof(win_trans), "/mnt/%c/%s", drive, resolved + 3);
        } else {
            snprintf(win_trans, sizeof(win_trans), "/mnt/%c", drive);
        }
        strncpy(resolved, win_trans, sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = '\0';
    }

    if (chdir(resolved) != 0) { fprintf(stderr, "cd: %s: %s\n", target, strerror(errno)); return 1; }
    if (old[0]) setenv("OLDPWD", old, 1);
    return 0;
}

extern char **environ;

static int bi_export(int argc, char **argv)
{
    if (argc == 1) {
        if (!environ) return 0;
        for (char **e = environ; *e; e++) {
            printf("export %s\n", *e);
        }
        return 0;
    }

    int ret = 0;
    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            const char *name = argv[i];
            const char *val = eq + 1;
            if (!isalpha((unsigned char)*name) && *name != '_') {
                fprintf(stderr, "export: `%s=%s': not a valid identifier\n", name, val);
                ret = 1;
            } else {
                int valid = 1;
                for (const char *c = name + 1; *c; c++) {
                    if (!isalnum((unsigned char)*c) && *c != '_') { valid = 0; break; }
                }
                if (!valid) {
                    fprintf(stderr, "export: `%s=%s': not a valid identifier\n", name, val);
                    ret = 1;
                } else {
                    if (setenv(name, val, 1) != 0) {
                        perror("export: setenv");
                        ret = 1;
                    }
                }
            }
            *eq = '=';
        } else {
            const char *name = argv[i];
            if (!isalpha((unsigned char)*name) && *name != '_') {
                fprintf(stderr, "export: `%s': not a valid identifier\n", name);
                ret = 1;
            } else {
                int valid = 1;
                for (const char *c = name + 1; *c; c++) {
                    if (!isalnum((unsigned char)*c) && *c != '_') { valid = 0; break; }
                }
                if (!valid) {
                    fprintf(stderr, "export: `%s': not a valid identifier\n", name);
                    ret = 1;
                } else {
                    const char *existing = getenv(name);
                    if (existing) setenv(name, existing, 1);
                }
            }
        }
    }
    return ret;
}

static int bi_unset(int argc, char **argv)
{
    if (argc < 2) return 0;
    for (int i = 1; i < argc; i++) {
        unsetenv(argv[i]);
    }
    return 0;
}

#define MAX_JOBS 32

typedef struct {
    int   id;            /* job id: 1, 2, ... */
    pid_t pid;           /* process id */
    char  cmd[128];      /* command line string */
    int   active;        /* 1 = running, 0 = empty */
} Job;

static Job jobs[MAX_JOBS];
static int next_job_id = 1;

static void reap_jobs(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) continue;
        int status;
        pid_t p = waitpid(jobs[i].pid, &status, WNOHANG);
        if (p > 0) {
            set_fg(GREEN); printf("[%d]+  Done                    ", jobs[i].id);
            set_fg(WHITE); printf("%s\n" RESET, jobs[i].cmd);
            fflush(stdout);
            jobs[i].active = 0;
        } else if (p < 0 && errno == ECHILD) {
            jobs[i].active = 0;
        }
    }
}

static void add_job(pid_t pid, const char *cmd)
{
    int slot = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        reap_jobs();
        for (int i = 0; i < MAX_JOBS; i++) {
            if (!jobs[i].active) { slot = i; break; }
        }
    }
    if (slot < 0) {
        fprintf(stderr, "retroshell: job table full\n");
        return;
    }

    jobs[slot].id = next_job_id++;
    jobs[slot].pid = pid;
    jobs[slot].active = 1;
    strncpy(jobs[slot].cmd, cmd ? cmd : "unknown", sizeof(jobs[slot].cmd) - 1);
    jobs[slot].cmd[sizeof(jobs[slot].cmd) - 1] = '\0';

    set_fg(AMBER); printf("[%d] ", jobs[slot].id);
    set_fg(WHITE); printf("%d\n" RESET, (int)pid);
    fflush(stdout);
}

static int bi_jobs(void)
{
    reap_jobs();
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            set_fg(AMBER); printf("[%d]+  ", jobs[i].id);
            set_fg(CYAN);  printf("Running                 ");
            set_fg(WHITE); printf("%s &\n" RESET, jobs[i].cmd);
        }
    }
    return 0;
}

static void format_cmd_string(char **tok, int start, int end, char *out, size_t out_sz)
{
    out[0] = '\0';
    size_t len = 0;
    for (int i = start; i < end; i++) {
        size_t tlen = strlen(tok[i]);
        if (len + tlen + 2 >= out_sz) break;
        if (i > start) out[len++] = ' ';
        memcpy(out + len, tok[i], tlen);
        len += tlen;
        out[len] = '\0';
    }
}

#define MAX_HIST 256
static char *cmd_history[MAX_HIST];
static int cmd_hist_count = 0;

static void hist_add(const char *cmd)
{
    if (!cmd || !*cmd) return;
    if (cmd_hist_count > 0 && strcmp(cmd_history[cmd_hist_count - 1], cmd) == 0)
        return;
    if (cmd_hist_count == MAX_HIST) {
        free(cmd_history[0]);
        for (int i = 1; i < MAX_HIST; i++)
            cmd_history[i - 1] = cmd_history[i];
        cmd_hist_count--;
    }
    cmd_history[cmd_hist_count++] = strdup(cmd);
}

static int bi_history(void)
{
    for (int i = 0; i < cmd_hist_count; i++) {
        set_fg(MUTED); printf(" %4d  ", i + 1);
        set_fg(WHITE); printf("%s\n" RESET, cmd_history[i]);
    }
    return 0;
}

static int bi_user(int argc, char **argv)
{
    const char *home = getenv("HOME");
    char path[PATH_MAX];
    if (home && *home) snprintf(path, sizeof(path), "%s/.retroshell_user", home);
    else strcpy(path, ".retroshell_user");

    if (argc == 1 || (argc == 2 && strcmp(argv[1], "info") == 0)) {
        set_fg((RGB){0xc0, 0x84, 0xfc});
        printf(BOLD "\n  [ RetroOS User Profile ]\n" RESET);
        set_fg(MUTED); printf("  ──────────────────────────────────────────\n");
        set_fg(CYAN);  printf("  • Active Identity : ");
        set_fg(WHITE); printf(BOLD "%s@retros\n" RESET, g_username[0] ? g_username : "retro");
        set_fg(CYAN);  printf("  • Profile Store   : ");
        set_fg(MUTED); printf("%s\n", path);
        set_fg(MUTED); printf("  ──────────────────────────────────────────\n");
        set_fg((RGB){0xfa, 0xb3, 0x87});
        printf("  Management Options:\n");
        set_fg(GREEN); printf("    user rename <new_name> ");
        set_fg(MUTED); printf("-> Change your username\n");
        set_fg(RED);   printf("    user delete            ");
        set_fg(MUTED); printf("-> Delete profile (prompts on next launch)\n");
        set_fg(CYAN);  printf("    welcome                ");
        set_fg(MUTED); printf("-> Open Page 1 onboarding setup\n\n" RESET);
        return 0;
    }

    if (strcmp(argv[1], "delete") == 0 || strcmp(argv[1], "remove") == 0 || strcmp(argv[1], "reset") == 0) {
        unlink(path);
        g_username[0] = '\0';
        set_fg(RED);
        printf(BOLD "[*] Account profile deleted successfully!\n" RESET);
        set_fg(MUTED);
        printf("    When you or your friend restarts RetroOS, Page 1 will ask to create a new profile.\n" RESET);
        return 0;
    }

    const char *new_name = (strcmp(argv[1], "rename") == 0 && argc >= 3) ? argv[2] : argv[1];
    if (strcmp(new_name, "rename") == 0) {
        set_fg(RED); printf("Usage: user rename <new_name>\n" RESET);
        return 1;
    }

    strncpy(g_username, new_name, sizeof(g_username) - 1);
    g_username[sizeof(g_username) - 1] = '\0';

    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s\n", g_username);
        fclose(f);
    }
    set_fg(GREEN);
    printf("[*] Username updated to: ");
    set_fg(WHITE); printf(BOLD "%s@retros\n" RESET, g_username);
    return 0;
}

static int bi_welcome(void)
{
    show_welcome_page();
    set_fg((RGB){0xa6, 0xe3, 0xa1});
    printf(BOLD "  [?] Enter username (press Enter to keep \"%s\"): " RESET "\x1b[38;2;245;245;245m",
           g_username[0] ? g_username : "retro");
    fflush(stdout);

    char input[128];
    if (fgets(input, sizeof(input), stdin)) {
        char *p = input;
        while (isspace((unsigned char)*p)) p++;
        size_t l = strlen(p);
        while (l > 0 && (p[l - 1] == '\n' || p[l - 1] == '\r' || isspace((unsigned char)p[l - 1])))
            p[--l] = '\0';
        if (*p) {
            strncpy(g_username, p, sizeof(g_username) - 1);
            g_username[sizeof(g_username) - 1] = '\0';
            const char *home = getenv("HOME");
            char path[PATH_MAX];
            if (home && *home) snprintf(path, sizeof(path), "%s/.retroshell_user", home);
            else strcpy(path, ".retroshell_user");
            FILE *f = fopen(path, "w");
            if (f) {
                fprintf(f, "%s\n", g_username);
                fclose(f);
            }
        }
    }
    printf(RESET);
    printf("\x1b[2J\x1b[H");
    print_banner();
    fflush(stdout);
    return 0;
}

static int bi_help(void)
{
    static const char *rows[][2] = {
        {"cd [dir]",              "change directory (cd -, cd ~ also work)"},
        {"pwd",                   "print working directory"},
        {"welcome",               "view Page 1 welcome screen (for screenshot/setup)"},
        {"user [name]",           "view or change your shell username"},
        {"export [NAME=val]",     "set environment variable (export alone lists all)"},
        {"unset NAME",            "remove environment variable"},
        {"jobs",                  "list active background jobs"},
        {"history",               "show previous command history"},
        {"cmd &",                 "run in background (e.g. sleep 5 &)"},
        {"cmd > file",            "write output to file (>> to append)"},
        {"cmd < file",            "read input from file"},
        {"cmd1 | cmd2",           "pipe: output of cmd1 becomes input of cmd2"},
        {"cmd1 ; cmd2",           "run sequentially (run cmd1, then cmd2)"},
        {"cmd1 && cmd2",          "run cmd2 only if cmd1 succeeded (exit 0)"},
        {"cmd1 || cmd2",          "run cmd2 only if cmd1 failed (non-zero exit)"},
        {"sched fcfs [AT:BT ...]","simulated FCFS + Gantt chart  e.g. sched fcfs 0:5 1:3 2:8"},
        {"sched real fcfs|rr ...","REAL scheduler on real programs, live Gantt chart"},
        {"",                      "  e.g. sched real rr 0.5 \"burn 2\" \"burn 1\" \"burn 3\""},
        {"monitor [sec] | once",  "LIVE CPU / RAM / process dashboard from /proc (q quits)"},
        {"mode-switch",           "toggle simulated privilege level (Ring 3 <-> Ring 0)"},
        {"theme <name>",          "retro, cyber, matrix, dracula"},
        {"fastfetch",             "display the Cyber Purple Cat & system specs"},
        {"clear | cls",           "clear the screen and history"},
        {"dir [path]",            "list directory files (Windows CMD alias)"},
        {"C: | E: | D:",          "switch directly to Windows drive"},
        {"type <file>",           "display contents of a file (alias to cat)"},
        {"md | rd | del",         "mkdir, rmdir, and delete files (CMD aliases)"},
        {"copy | move | ren",     "copy, move, and rename files (CMD aliases)"},
        {"exit [code]",           "leave the shell"},
    };
    printf("\n"); set_fg(CYAN); printf(BOLD "RetroOS built-in commands:\n" RESET);
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        set_fg(AMBER); printf(BOLD "  %-26s" RESET, rows[i][0]);
        set_fg(MUTED); printf("%s\n" RESET, rows[i][1]);
    }
    set_fg(MUTED);
    printf("  Any other command (ls, gcc, python3 ...) is run as a real program.\n" RESET);
    return 0;
}

static int bi_theme(int argc, char **argv)
{
    if (argc == 2)
        for (int i = 0; i < NTHEMES; i++)
            if (strcmp(argv[1], THEMES[i].name) == 0) {
                theme_idx = i;
                set_fg(THEMES[i].accent);
                printf("Theme changed to '%s'.\n" RESET, THEMES[i].name);
                return 0;
            }
    set_fg(CYAN); printf("Available themes:");
    for (int i = 0; i < NTHEMES; i++) printf(" %s", THEMES[i].name);
    printf("\n" RESET "Usage: theme dracula\n");
    return argc == 1 ? 0 : 1;
}

static int bi_mode_switch(void)
{
    kernel_mode = !kernel_mode;
    if (kernel_mode) { set_fg(RED);   printf(BOLD "[!] HARDWARE TRAP: Escalated to RING 0 (simulated)\n" RESET); }
    else             { set_fg(GREEN); printf(BOLD "[*] DROPPING PRIVILEGES: Switched to RING 3 (simulated)\n" RESET); }
    return 0;
}

/* ---------------------------- sched fcfs --------------------------- */
typedef struct { int idx, at, bt, start, ct, tat, wt; } Proc;

static void center(const char *s, int w)
{
    int len = (int)strlen(s);
    if (len > w) len = w;
    int left = (w - len) / 2;
    printf("%*s%.*s%*s", left, "", len, s, w - len - left, "");
}

static int sched_real(int n, char **a);

static int bi_sched(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "real") == 0) return sched_real(argc - 2, argv + 2);
    if (argc < 2 || strcmp(argv[1], "fcfs") != 0) {
        fprintf(stderr, "Usage: sched fcfs [AT:BT ...]   e.g. sched fcfs 0:5 1:3 2:8\n"
                        "       sched real fcfs|rr ...    runs REAL programs (see: sched real)\n");
        return 1;
    }

    Proc procs[MAX_PROCS];
    int n = 0;
    if (argc == 2) {                                   /* demo data */
        int demo[][2] = {{0,5},{1,3},{2,8}};
        for (; n < 3; n++) { procs[n].at = demo[n][0]; procs[n].bt = demo[n][1]; }
    } else {
        for (int i = 2; i < argc; i++) {
            if (n >= MAX_PROCS) { fprintf(stderr, "sched: at most %d processes\n", MAX_PROCS); return 1; }
            char *e1, *e2 = NULL;
            long at = strtol(argv[i], &e1, 10);
            long bt = (e1 != argv[i] && *e1 == ':') ? strtol(e1 + 1, &e2, 10) : 0;
            if (e1 == argv[i] || *e1 != ':' || e2 == e1 + 1 || *e2 != '\0' || at < 0 || bt < 1) {
                fprintf(stderr, "sched: invalid process '%s' (use AT:BT, BT >= 1)\n", argv[i]);
                return 1;
            }
            procs[n].at = (int)at; procs[n].bt = (int)bt; n++;
        }
    }
    for (int i = 0; i < n; i++) procs[i].idx = i;

    /* FCFS = serve in order of arrival. Stable insertion sort by AT. */
    int order[MAX_PROCS];
    for (int i = 0; i < n; i++) {
        int j = i;
        while (j > 0 && procs[order[j-1]].at > procs[i].at) { order[j] = order[j-1]; j--; }
        order[j] = i;
    }

    /* Gantt blocks (processes and idle gaps) */
    struct { int pid, from, to; } blocks[2 * MAX_PROCS];
    int nb = 0, t = 0;
    double sum_wt = 0, sum_tat = 0;
    for (int k = 0; k < n; k++) {
        Proc *p = &procs[order[k]];
        if (t < p->at) { blocks[nb].pid = -1; blocks[nb].from = t; blocks[nb].to = p->at; nb++; t = p->at; }
        p->start = t;
        p->ct    = t + p->bt;
        p->tat   = p->ct - p->at;
        p->wt    = p->tat - p->bt;
        sum_wt += p->wt; sum_tat += p->tat;
        blocks[nb].pid = p->idx; blocks[nb].from = p->start; blocks[nb].to = p->ct; nb++;
        t = p->ct;
    }

    const Theme *th = &THEMES[theme_idx];
    printf("\n"); set_fg(AMBER);
    printf(BOLD "CPU SCHEDULER: First-Come First-Served (FCFS)\n" RESET);
    set_fg(CYAN);
    printf(BOLD "  %-8s %-6s %-6s %-6s %-6s %-6s\n" RESET, "Process", "AT", "BT", "CT", "TAT", "WT");
    for (int i = 0; i < n; i++) {
        set_fg(th->accent); printf(BOLD "  P%-7d" RESET, i + 1);
        printf(" %-6d %-6d %-6d %-6d %-6d\n", procs[i].at, procs[i].bt, procs[i].ct, procs[i].tat, procs[i].wt);
    }

    printf("\n"); set_fg((RGB){0xc4,0xb5,0xfd}); printf(BOLD "Gantt Chart:\n" RESET "  ");
    int widths[2 * MAX_PROCS];
    for (int b = 0; b < nb; b++) {
        int w = (blocks[b].to - blocks[b].from) * 3;
        if (w < 6) w = 6;
        if (w > 36) w = 36;
        widths[b] = w;
        char label[16];
        if (blocks[b].pid < 0) { set_bg((RGB){0x33,0x41,0x55}); set_fg(MUTED); snprintf(label, sizeof label, "idle"); }
        else { set_bg(GANTT[blocks[b].pid % NGANTT]); set_fg(WHITE); snprintf(label, sizeof label, "P%d", blocks[b].pid + 1); }
        printf(BOLD); center(label, w); printf(RESET);
    }
    printf("\n  ");
    set_fg(MUTED);
    for (int b = 0; b < nb; b++) { char num[16]; snprintf(num, sizeof num, "%d", blocks[b].from); printf("%-*s", widths[b], num); }
    printf("%d\n" RESET, blocks[nb - 1].to);

    set_fg(GREEN);
    printf(BOLD "\n  \xe2\x9c\x94 Average Waiting Time:    %.2f ms\n  \xe2\x9c\x94 Average Turnaround Time: %.2f ms\n" RESET,
           sum_wt / n, sum_tat / n);
    return 0;
}


/* ------------------------------------------------------------------
 * sched real  --  a REAL CPU scheduler
 *
 * Unlike `sched fcfs 0:5 1:3` (numbers in, chart out), this starts
 * real programs and controls them with signals:
 *     SIGSTOP  = take the CPU away      SIGCONT = give the CPU
 * Every program lives in its own process group, so a whole pipeline
 * or a `sh -c` tree is paused/resumed together. Only ONE group is
 * allowed to run at a time, just like one CPU core.
 *
 *   FCFS : run each program to completion, in the order given
 *   RR   : give each program `quantum` seconds, then preempt it and
 *          move to the next one (Round Robin)
 *
 * Times are measured with the real clock; CPU(s) comes from the
 * kernel (getrusage). "burn N" is a built-in CPU hog that burns N
 * seconds of CPU, handy for demos.
 * ------------------------------------------------------------------ */
typedef struct {
    pid_t pid;
    const char *cmd;
    int started, done, exit_code, slices;
    double first_run, ct, run, cpu;
} RProc;

static int g_col, g_width;

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static double children_cpu(void)
{
    struct rusage ru;
    getrusage(RUSAGE_CHILDREN, &ru);
    return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 +
           ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
}

static void burn_cpu(double secs)       /* spin until we have used `secs` of CPU time */
{
    struct timespec ts;
    for (;;) {
        for (volatile long k = 0; k < 200000; k++) { }
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
        if ((double)ts.tv_sec + ts.tv_nsec / 1e9 >= secs) break;
    }
}

static void gantt_put(RGB c, const char *txt)      /* one coloured cell, wraps at screen width */
{
    if (g_col >= g_width - 4) {
        printf(RESET "\n  ");
        g_col = 0;
        set_bg(c); set_fg(WHITE); printf(BOLD);
    }
    printf("%s", txt);
    g_col += (int)strlen(txt);
}

/* Let process p own the CPU. quantum < 0 = until it finishes.
 * Returns 1 = finished, 0 = preempted, -1 = Ctrl+C. */
static int run_slice(RProc *p, int idx, double quantum, double t0)
{
    RGB c = GANTT[idx % NGANTT];
    double s0 = now_s();
    if (!p->started) { p->started = 1; p->first_run = s0 - t0; }
    p->slices++;

    char label[16];
    snprintf(label, sizeof label, "P%d", idx + 1);
    set_bg(c); set_fg(WHITE); printf(BOLD);
    gantt_put(c, label);
    int cells = (int)strlen(label);          /* 1 cell = 0.1 s */
    fflush(stdout);

    kill(-p->pid, SIGCONT);                  /* give it the CPU */

    int finished = 0, st = 0;
    for (;;) {
        sleep_ms(10);
        if (got_sigint) { printf(RESET); return -1; }

        double before = children_cpu();
        pid_t r = waitpid(p->pid, &st, WNOHANG);
        if (r == p->pid) { p->cpu = children_cpu() - before; finished = 1; break; }

        double el = now_s() - s0;
        int target = (int)(el / 0.1);
        while (cells < target) { gantt_put(c, " "); cells++; }
        fflush(stdout);

        if (quantum >= 0 && el >= quantum) {          /* time slice used up: preempt */
            kill(-p->pid, SIGSTOP);
            double b2 = children_cpu();
            pid_t r2;
            do { r2 = waitpid(p->pid, &st, WUNTRACED); } while (r2 < 0 && errno == EINTR);
            if (r2 == p->pid && (WIFEXITED(st) || WIFSIGNALED(st))) {
                p->cpu = children_cpu() - b2;         /* it exited just as we stopped it */
                finished = 1;
            }
            break;
        }
    }

    double s1 = now_s();
    int target = (int)((s1 - s0) / 0.1 + 0.5);
    while (cells < target) { gantt_put(c, " "); cells++; }
    printf(RESET); fflush(stdout);

    p->run += s1 - s0;
    if (finished) {
        p->done = 1;
        p->ct = s1 - t0;
        p->exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
    }
    return finished;
}

static void sched_real_usage(void)
{
    fprintf(stderr,
        "Usage: sched real [-o] fcfs \"cmd\" [\"cmd\" ...]\n"
        "       sched real [-o] rr [quantum_sec] \"cmd\" [\"cmd\" ...]\n"
        "  e.g. sched real rr 0.5 \"burn 2\" \"burn 1\" \"burn 3\"\n"
        "  'burn N' = built-in CPU hog (uses N seconds of CPU).\n"
        "  Any command works:  sched real rr 1 \"gcc big.c\" \"python3 a.py\"\n"
        "  Program output is hidden so it does not mess up the chart; -o shows it.\n");
}

static int sched_real(int n, char **a)
{
    int show_out = 0;
    while (n > 0 && strcmp(a[0], "-o") == 0) { show_out = 1; a++; n--; }
    if (n < 2) { sched_real_usage(); return 1; }

    int rr;
    if      (strcmp(a[0], "fcfs") == 0) rr = 0;
    else if (strcmp(a[0], "rr") == 0)   rr = 1;
    else { fprintf(stderr, "sched real: unknown algorithm '%s' (use fcfs or rr)\n", a[0]); return 1; }
    a++; n--;

    double quantum = 0.5;
    if (rr && n > 0) {
        char *e;
        double q = strtod(a[0], &e);
        if (e != a[0] && *e == '\0') {
            if (q < 0.05 || q > 60) { fprintf(stderr, "sched real: quantum must be between 0.05 and 60 seconds\n"); return 1; }
            quantum = q; a++; n--;
        }
    }
    if (n < 1 || n > MAX_PROCS) { sched_real_usage(); return 1; }

    for (int i = 0; i < n; i++)
        if (strncmp(a[i], "burn", 4) == 0 && (a[i][4] == ' ' || a[i][4] == '\0')) {
            char *e; double v = strtod(a[i] + 4, &e);
            if (e == a[i] + 4 || *e != '\0' || v <= 0 || v > 600) {
                fprintf(stderr, "sched real: usage is \"burn SECONDS\" (0 < SECONDS <= 600)\n");
                return 1;
            }
        }

    struct winsize ws;
    g_width = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 20) ? ws.ws_col : 80;
    g_col = 0;
    got_sigint = 0;

    /* ---- start every program, but frozen (SIGSTOP) until its turn ---- */
    RProc procs[MAX_PROCS];
    memset(procs, 0, sizeof procs);
    fflush(stdout);
    for (int i = 0; i < n; i++) {
        procs[i].cmd = a[i];
        pid_t pid = fork();
        if (pid < 0) {
            perror("sched real: fork");
            for (int j = 0; j < i; j++) { kill(-procs[j].pid, SIGKILL); waitpid(procs[j].pid, NULL, 0); }
            return 1;
        }
        if (pid == 0) {
            setpgid(0, 0);                       /* own process group */
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            int dn = open("/dev/null", O_RDWR);
            if (dn >= 0) {
                dup2(dn, STDIN_FILENO);
                if (!show_out) { dup2(dn, STDOUT_FILENO); dup2(dn, STDERR_FILENO); }
                if (dn > 2) close(dn);
            }
            raise(SIGSTOP);                      /* wait here until the scheduler says go */
            if (strncmp(a[i], "burn", 4) == 0 && (a[i][4] == ' ' || a[i][4] == '\0')) {
                burn_cpu(strtod(a[i] + 4, NULL));
                _exit(0);
            }
            execl("/bin/sh", "sh", "-c", a[i], (char *)NULL);
            _exit(127);
        }
        procs[i].pid = pid;
        setpgid(pid, pid);
        int st;
        waitpid(pid, &st, WUNTRACED);            /* make sure it is frozen */
    }

    const Theme *th = &THEMES[theme_idx];
    printf("\n"); set_fg(AMBER);
    if (rr) printf(BOLD "REAL CPU SCHEDULER: Round Robin (quantum %.2f s), %d real processes\n" RESET, quantum, n);
    else    printf(BOLD "REAL CPU SCHEDULER: First-Come First-Served, %d real processes\n" RESET, n);
    for (int i = 0; i < n; i++) {
        set_bg(GANTT[i % NGANTT]); set_fg(WHITE); printf(BOLD " P%d " RESET, i + 1);
        set_fg(MUTED); printf(" %s\n" RESET, procs[i].cmd);
    }
    set_fg((RGB){0xc4,0xb5,0xfd}); printf(BOLD "\nGantt Chart (live, 1 cell = 0.1 s):\n" RESET "  ");
    fflush(stdout);

    /* ---- the scheduler loop ---- */
    int queue[MAX_PROCS + 1], qh = 0, qt = 0, qlen = 0;
    for (int i = 0; i < n; i++) { queue[qt] = i; qt = (qt + 1) % (MAX_PROCS + 1); qlen++; }
    int remaining = n, last = -1, switches = 0, aborted = 0;
    double t0 = now_s();

    while (remaining > 0) {
        int i = queue[qh]; qh = (qh + 1) % (MAX_PROCS + 1); qlen--;
        double q = (rr && qlen > 0) ? quantum : -1;    /* last one left: no need to preempt */
        if (last >= 0 && last != i) switches++;
        last = i;

        int r = run_slice(&procs[i], i, q, t0);
        if (r < 0) { aborted = 1; break; }
        if (r == 1) remaining--;
        else { queue[qt] = i; qt = (qt + 1) % (MAX_PROCS + 1); qlen++; }
    }
    double wall = now_s() - t0;

    if (aborted) {                                      /* Ctrl+C: kill everything */
        for (int i = 0; i < n; i++)
            if (!procs[i].done) { kill(-procs[i].pid, SIGKILL); waitpid(procs[i].pid, NULL, 0); }
        set_fg(RED); printf(BOLD "\n[sched] interrupted: all processes killed\n" RESET);
        return 130;
    }

    /* ---- results ---- */
    set_fg(CYAN);
    printf(BOLD "\n\n  %-8s %-8s %-8s %-8s %-8s %-8s %s\n" RESET, "Process", "CPU(s)", "CT", "TAT", "WT", "RT", "Command");
    double sw = 0, st_ = 0, sr = 0, scpu = 0;
    for (int i = 0; i < n; i++) {
        RProc *p = &procs[i];
        double tat = p->ct, wt = p->ct - p->run;
        sw += wt; st_ += tat; sr += p->first_run; scpu += p->cpu;
        set_fg(th->accent); printf(BOLD "  P%-7d" RESET, i + 1);
        printf(" %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f %s", p->cpu, p->ct, tat, wt, p->first_run, p->cmd);
        if (p->exit_code != 0) { set_fg(RED); printf("  [exit %d]", p->exit_code); }
        printf(RESET "\n");
    }
    set_fg(GREEN);
    printf(BOLD "\n  \xe2\x9c\x94 Average Waiting Time:    %.2f s\n"
                "  \xe2\x9c\x94 Average Turnaround Time: %.2f s\n"
                "  \xe2\x9c\x94 Average Response Time:   %.2f s\n" RESET, sw / n, st_ / n, sr / n);
    set_fg(MUTED);
    printf("  Total time %.2f s | context switches %d | CPU busy %.0f%% | throughput %.2f proc/s\n",
           wall, switches, wall > 0 ? 100.0 * scpu / wall : 0.0, wall > 0 ? n / wall : 0.0);
    printf("  All processes arrive at t=0. CPU(s) is measured by the kernel; WT = TAT - time held on the CPU.\n" RESET);
    return 0;
}

/* ------------------------------------------------------------------
 * monitor  --  live system dashboard (a tiny `top`)
 *
 * Everything comes straight from the kernel's /proc files:
 *   /proc/stat      CPU time per core   -> usage % (change between 2 samples)
 *   /proc/meminfo   RAM and swap
 *   /proc/loadavg, /proc/uptime
 *   /proc/<pid>/stat, cmdline  -> every process: state, CPU %, RAM
 *
 *   monitor            live view, refresh every 1 s
 *   monitor 0.5        refresh every 0.5 s
 *   monitor once       print one snapshot and return (works in pipes: monitor once | head)
 * keys: q quit   c sort by CPU   m sort by memory
 * ------------------------------------------------------------------ */
#define MON_MAX_CPUS  64
#define MON_MAX_PROCS 8192

typedef struct { unsigned long long total, idle; } CpuT;
typedef struct { int pid; unsigned long long ticks; } PTick;
typedef struct {
    int pid;
    char state, user[12], comm[32];
    unsigned long long ticks;
    long rss_kb;
    double cpu, mem;
} MProc;

typedef struct {
    int ncpu;                                /* real cores (not counting the total line) */
    double cpu_pct[MON_MAX_CPUS + 1];        /* [0] = all cores together */
    double mem_total, mem_avail, swap_total, swap_free;   /* kB */
    int nproc, nrun;
    double uptime, load[3];
} MonSnap;

static MProc  g_mp[MON_MAX_PROCS];
static PTick  g_prev_p[MON_MAX_PROCS];
static CpuT   g_prev_cpu[MON_MAX_CPUS + 1];
static int    g_prev_np = 0, g_prev_ncpu = 0, g_have_prev = 0, g_sort = 0;
static double g_prev_t = 0;

static int read_small(const char *path, char *buf, size_t n)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = read(fd, buf, n - 1);
    close(fd);
    if (r < 0) return -1;
    buf[r] = '\0';
    return (int)r;
}

static const char *uid_name(uid_t uid)
{
    static struct { uid_t uid; char name[16]; } cache[64];
    static int nc = 0;
    static char tmp[16];
    for (int i = 0; i < nc; i++) if (cache[i].uid == uid) return cache[i].name;
    struct passwd *pw = getpwuid(uid);
    char *dst = (nc < 64) ? cache[nc].name : tmp;
    if (pw) snprintf(dst, 16, "%s", pw->pw_name); else snprintf(dst, 16, "%u", (unsigned)uid);
    if (nc < 64) cache[nc++].uid = uid;
    return dst;
}

static int mon_read_cpu(CpuT *out)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0;
    char line[512];
    int n = 0;
    while (n <= MON_MAX_CPUS && fgets(line, sizeof line, f) && strncmp(line, "cpu", 3) == 0) {
        char name[16];
        unsigned long long v[8] = {0};
        if (sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu", name,
                   &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) < 5) break;
        out[n].total = v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6]+v[7];
        out[n].idle  = v[3] + v[4];
        n++;
    }
    fclose(f);
    return n;
}

static void mon_read_mem(MonSnap *s)
{
    FILE *f = fopen("/proc/meminfo", "r");
    s->mem_total = s->mem_avail = s->swap_total = s->swap_free = 0;
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        double v;
        if      (sscanf(line, "MemTotal: %lf", &v) == 1)     s->mem_total = v;
        else if (sscanf(line, "MemAvailable: %lf", &v) == 1) s->mem_avail = v;
        else if (sscanf(line, "SwapTotal: %lf", &v) == 1)    s->swap_total = v;
        else if (sscanf(line, "SwapFree: %lf", &v) == 1)     s->swap_free = v;
    }
    fclose(f);
}

static int mon_scan(MProc *pr, int max, long page_kb)
{
    DIR *d = opendir("/proc");
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while (n < max && (e = readdir(d)) != NULL) {
        if (e->d_name[0] < '1' || e->d_name[0] > '9') continue;
        int pid = atoi(e->d_name);
        char path[64], buf[1024];
        snprintf(path, sizeof path, "/proc/%d/stat", pid);
        if (read_small(path, buf, sizeof buf) <= 0) continue;      /* process vanished */
        char *lp = strchr(buf, '('), *rp = strrchr(buf, ')');      /* name may contain spaces */
        if (!lp || !rp || rp < lp) continue;

        MProc *p = &pr[n];
        memset(p, 0, sizeof *p);
        size_t cl = (size_t)(rp - lp - 1);
        if (cl > sizeof p->comm - 1) cl = sizeof p->comm - 1;
        memcpy(p->comm, lp + 1, cl);

        unsigned long ut, st;
        long rss;
        char state;
        if (sscanf(rp + 1, " %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu "
                           "%*d %*d %*d %*d %*d %*d %*u %*u %ld",
                   &state, &ut, &st, &rss) != 4) continue;
        p->pid = pid;
        p->state = state;
        p->ticks = (unsigned long long)ut + st;
        p->rss_kb = rss * page_kb;

        snprintf(path, sizeof path, "/proc/%d", pid);
        struct stat sb;
        snprintf(p->user, sizeof p->user, "%s", stat(path, &sb) == 0 ? uid_name(sb.st_uid) : "?");
        n++;
    }
    closedir(d);
    return n;
}

/* Take one sample. CPU % = how much the counters moved since the previous sample. */
static int mon_sample(MonSnap *s)
{
    long hz = sysconf(_SC_CLK_TCK);       if (hz <= 0) hz = 100;
    long page_kb = sysconf(_SC_PAGESIZE) / 1024; if (page_kb <= 0) page_kb = 4;
    double t = now_s();

    CpuT cur[MON_MAX_CPUS + 1];
    int nc = mon_read_cpu(cur);
    memset(s, 0, sizeof *s);
    s->ncpu = nc > 0 ? nc - 1 : 0;
    for (int i = 0; i < nc; i++) {
        if (g_have_prev && i < g_prev_ncpu) {
            double dt = (double)(cur[i].total - g_prev_cpu[i].total);
            double di = (double)(cur[i].idle  - g_prev_cpu[i].idle);
            s->cpu_pct[i] = dt > 0 ? 100.0 * (dt - di) / dt : 0;
        }
        g_prev_cpu[i] = cur[i];
    }
    g_prev_ncpu = nc;

    mon_read_mem(s);

    int n = mon_scan(g_mp, MON_MAX_PROCS, page_kb);
    double dts = t - g_prev_t;
    for (int i = 0; i < n; i++) {
        MProc *p = &g_mp[i];
        unsigned long long old = 0;
        int found = 0;
        for (int j = 0; j < g_prev_np; j++)
            if (g_prev_p[j].pid == p->pid) { old = g_prev_p[j].ticks; found = 1; break; }
        double dticks = (double)(found ? p->ticks - old : p->ticks);
        p->cpu = (g_have_prev && dts > 0) ? 100.0 * dticks / hz / dts : 0;
        p->mem = s->mem_total > 0 ? 100.0 * p->rss_kb / s->mem_total : 0;
        if (p->state == 'R') s->nrun++;
    }
    g_prev_np = n;
    for (int i = 0; i < n; i++) { g_prev_p[i].pid = g_mp[i].pid; g_prev_p[i].ticks = g_mp[i].ticks; }
    s->nproc = n;
    g_prev_t = t;
    g_have_prev = 1;

    char buf[128];
    if (read_small("/proc/uptime", buf, sizeof buf) > 0) sscanf(buf, "%lf", &s->uptime);
    if (read_small("/proc/loadavg", buf, sizeof buf) > 0) sscanf(buf, "%lf %lf %lf", &s->load[0], &s->load[1], &s->load[2]);
    return n;
}

static int mon_cmp(const void *a, const void *b)
{
    const MProc *x = a, *y = b;
    double kx = g_sort ? x->mem : x->cpu, ky = g_sort ? y->mem : y->cpu;
    if (kx < ky) return 1;
    if (kx > ky) return -1;
    return x->pid - y->pid;
}

static void mon_bar(double pct, int w)
{
    double f = pct / 100.0;
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    int fill = (int)(f * w + 0.5);
    set_bg(f < 0.5 ? GREEN : f < 0.8 ? AMBER : RED);
    printf("%*s", fill, "");                                      /* filled part */
    set_bg((RGB){0x1e, 0x29, 0x3b});
    printf("%*s", w - fill, "");                                  /* empty track (slate) */
    printf(RESET);
}

static void mon_cmdline(int pid, const char *comm, char *out, size_t sz)
{
    char path[64], buf[512];
    snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
    int len = read_small(path, buf, sizeof buf);
    if (len <= 0) { snprintf(out, sz, "[%s]", comm); return; }        /* kernel thread */
    for (int i = 0; i < len; i++) if ((unsigned char)buf[i] < 32) buf[i] = ' ';
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
    snprintf(out, sz, "%s", len > 0 ? buf : comm);
}

static void mon_size(char *out, size_t sz, double kb)
{
    if      (kb >= 1048576) snprintf(out, sz, "%.1fG", kb / 1048576);
    else if (kb >= 1024)    snprintf(out, sz, "%.0fM", kb / 1024);
    else                    snprintf(out, sz, "%.0fK", kb);
}

static void mon_hms(char *out, size_t sz, double secs)
{
    long t = (long)secs, d = t / 86400;
    t %= 86400;
    if (d > 0) snprintf(out, sz, "%ldd %02ld:%02ld:%02ld", d, t / 3600, (t % 3600) / 60, t % 60);
    else       snprintf(out, sz, "%02ld:%02ld:%02ld", t / 3600, (t % 3600) / 60, t % 60);
}

static void mon_render(const MonSnap *s, int W, int H, double interval, int live)
{
    const Theme *th = &THEMES[theme_idx];
    const char *eol = live ? "\x1b[K\n" : "\n";
    if (W < 40) W = 40;

    qsort(g_mp, (size_t)s->nproc, sizeof g_mp[0], mon_cmp);

    /* ---- title bar ---- */
    char up[32], clock_s[16], title[256];
    mon_hms(up, sizeof up, s->uptime);
    time_t now = time(NULL);
    strftime(clock_s, sizeof clock_s, "%H:%M:%S", localtime(&now));
    snprintf(title, sizeof title, " RetroOS Monitor   up %s   load %.2f %.2f %.2f   %s ",
             up, s->load[0], s->load[1], s->load[2], clock_s);
    set_bg(kernel_mode ? KERNEL_RED : th->seg1); set_fg(DARK);
    printf(BOLD "%-*.*s" RESET "%s", W, W, title, eol);

    /* ---- CPU: total + per core ---- */
    set_fg(th->accent); printf(BOLD " CPU " RESET);
    int bw = W - 5 - 8;
    if (bw < 10) bw = 10;
    mon_bar(s->cpu_pct[0], bw);
    set_fg(WHITE); printf(BOLD " %5.1f%%" RESET "%s", s->cpu_pct[0], eol);

    int show = s->ncpu < 16 ? s->ncpu : 16;
    int cols = W >= 90 ? 3 : 2;
    int cell = (W - 1 - 2 * (cols - 1)) / cols;
    int cbw = cell - 8;
    if (cbw < 4) cbw = 4;
    int corerows = 0;
    for (int i = 0; i < show; i += cols) {
        printf(" ");
        for (int c = 0; c < cols && i + c < show; c++) {
            int k = i + c;
            set_fg(MUTED); printf("%2d ", k);
            mon_bar(s->cpu_pct[k + 1], cbw);
            set_fg(MUTED); printf(" %3.0f%%" RESET "%s", s->cpu_pct[k + 1], (c < cols - 1 && i + c + 1 < show) ? "  " : "");
        }
        printf("%s", eol);
        corerows++;
    }

    /* ---- memory + swap ---- */
    double mu = s->mem_total - s->mem_avail;
    double mp = s->mem_total > 0 ? 100.0 * mu / s->mem_total : 0;
    int mbw = W - 5 - 25;
    if (mbw < 10) mbw = 10;
    set_fg(th->accent); printf(BOLD " MEM " RESET);
    mon_bar(mp, mbw);
    set_fg(WHITE); printf(" %5.2f/%5.2f GiB %3.0f%%" RESET "%s", mu / 1048576, s->mem_total / 1048576, mp, eol);
    int swap_line = s->swap_total > 0;
    if (swap_line) {
        double su = s->swap_total - s->swap_free;
        double sp = 100.0 * su / s->swap_total;
        set_fg(th->accent); printf(BOLD " SWP " RESET);
        mon_bar(sp, mbw);
        set_fg(WHITE); printf(" %5.2f/%5.2f GiB %3.0f%%" RESET "%s", su / 1048576, s->swap_total / 1048576, sp, eol);
    }

    /* ---- tasks line + table header ---- */
    set_fg(MUTED);
    printf(" Tasks: " RESET); set_fg(WHITE); printf(BOLD "%d" RESET, s->nproc);
    set_fg(MUTED); printf(" total, " RESET); set_fg(GREEN); printf(BOLD "%d" RESET, s->nrun);
    set_fg(MUTED); printf(" running   cores: %d   sorted by: " RESET, s->ncpu);
    set_fg(AMBER); printf(BOLD "%s" RESET "%s", g_sort ? "MEM" : "CPU", eol);

    char hdr[256];
    snprintf(hdr, sizeof hdr, " %7s %-11s %c %6s %6s %8s  %s", "PID", "USER", 'S', "CPU%", "MEM%", "RES", "COMMAND");
    set_bg(th->seg3); set_fg(DARK);
    printf(BOLD "%-*.*s" RESET "%s", W, W, hdr, eol);

    /* ---- processes ---- */
    int used = 1 + 1 + corerows + 1 + swap_line + 1 + 1 + 1;          /* lines above + footer */
    int rows = live ? H - used - 1 : 15;
    if (rows < 3) rows = 3;
    if (rows > s->nproc) rows = s->nproc;
    int cmdw = W - 48;
    if (cmdw < 10) cmdw = 10;
    for (int i = 0; i < rows; i++) {
        const MProc *p = &g_mp[i];
        char cmd[512], rss[16];
        mon_cmdline(p->pid, p->comm, cmd, sizeof cmd);
        mon_size(rss, sizeof rss, (double)p->rss_kb);
        set_fg(MUTED); printf(" %7d ", p->pid);
        set_fg(WHITE); printf("%-11.11s ", p->user);
        set_fg(p->state == 'R' ? GREEN : p->state == 'Z' ? RED : MUTED); printf("%c ", p->state);
        set_fg(p->cpu >= 50 ? RED : p->cpu >= 10 ? AMBER : WHITE); printf("%6.1f ", p->cpu);
        set_fg(p->mem >= 20 ? RED : p->mem >= 5 ? AMBER : WHITE);  printf("%6.1f ", p->mem);
        set_fg(WHITE); printf("%8s  ", rss);
        set_fg(th->accent); printf("%.*s" RESET "%s", cmdw, cmd, eol);
    }

    /* ---- footer ---- */
    if (live) {
        set_fg(MUTED);
        printf(" " RESET); set_fg(AMBER); printf(BOLD "q" RESET); set_fg(MUTED); printf(" quit   ");
        set_fg(AMBER); printf(BOLD "c" RESET); set_fg(MUTED); printf(" sort CPU   ");
        set_fg(AMBER); printf(BOLD "m" RESET); set_fg(MUTED); printf(" sort MEM   refresh %.1fs" RESET "\x1b[K", interval);
    }
    fflush(stdout);
}

static int bi_monitor(int argc, char **argv)
{
    double interval = 1.0;
    int once = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "once") == 0) once = 1;
        else {
            char *e;
            double v = strtod(argv[i], &e);
            if (e == argv[i] || *e != '\0' || v < 0.2 || v > 10) {
                fprintf(stderr, "Usage: monitor [seconds 0.2-10] | monitor once\n"
                                "  keys: q quit, c sort by CPU, m sort by memory\n");
                return 1;
            }
            interval = v;
        }
    }

    int live = !once && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    struct winsize ws;
    MonSnap snap;

    if (!live) {                                          /* single snapshot */
        int W = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 20) ? ws.ws_col : 100;
        g_have_prev = 0; g_sort = 0;
        mon_sample(&snap);                                /* baseline */
        sleep_ms(500);
        mon_sample(&snap);                                /* real numbers */
        mon_render(&snap, W, 0, interval, 0);
        return 0;
    }

    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);            /* read keys one by one, no echo */
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("\x1b[?1049h\x1b[?25l\x1b[2J");                /* alternate screen, hide cursor */
    fflush(stdout);

    got_sigint = 0;
    g_have_prev = 0; g_sort = 0;
    mon_sample(&snap);                                    /* baseline */
    sleep_ms(300);

    int quit = 0;
    while (!quit && !got_sigint) {
        mon_sample(&snap);
        {
            int W = 100, H = 30;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) { if (ws.ws_col > 20) W = ws.ws_col; if (ws.ws_row > 8) H = ws.ws_row; }
            printf("\x1b[H");
            mon_render(&snap, W - 1, H, interval, 1);
            printf("\x1b[J");
            fflush(stdout);
        }
        double deadline = now_s() + interval;
        while (!quit && !got_sigint) {
            double left = deadline - now_s();
            if (left <= 0) break;
            fd_set rf;
            FD_ZERO(&rf); FD_SET(STDIN_FILENO, &rf);
            struct timeval tv = { (time_t)left, (suseconds_t)((left - (long)left) * 1e6) };
            int r = select(STDIN_FILENO + 1, &rf, NULL, NULL, &tv);
            if (r > 0) {
                char ch;
                while (read(STDIN_FILENO, &ch, 1) == 1) {
                    if (ch == 'q' || ch == 'Q') quit = 1;
                    else if (ch == 'c' || ch == 'C') g_sort = 0;
                    else if (ch == 'm' || ch == 'M') g_sort = 1;
                }
                if (!quit) {                               /* show the new sort order at once */
                    int W = 100, H = 30;
                    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) { if (ws.ws_col > 20) W = ws.ws_col; if (ws.ws_row > 8) H = ws.ws_row; }
                    printf("\x1b[H");
                    mon_render(&snap, W - 1, H, interval, 1);
                    printf("\x1b[J");
                    fflush(stdout);
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);              /* give the terminal back */
    printf("\x1b[?25h\x1b[?1049l");
    fflush(stdout);
    got_sigint = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * OS Mode & Dual Filesystem Visualizer (for OS Academic & Demo)
 * ------------------------------------------------------------------ */
static int bi_os(int argc, char **argv)
{
    if (argc < 2) {
        printf("\n");
        set_fg((RGB){0x38, 0xbd, 0xf8});
        printf(BOLD "  Current Environment: %s\n" RESET, get_os_mode_name(g_os_mode));
        set_fg((RGB){0x94, 0xa3, 0xb8});
        printf("  Usage to switch mode:  os win | os linux | os hybrid\n");
        printf("  - os win    : Focuses on Windows drives and CMD toolset\n");
        printf("  - os linux  : Focuses on Linux root/home and Unix POSIX tools\n");
        printf("  - os hybrid : Blends both operating environments seamlessly\n\n");
        return 0;
    }

    const char *arg = argv[1];
    if (strcasecmp(arg, "win") == 0 || strcasecmp(arg, "windows") == 0) {
        g_os_mode = OS_MODE_WINDOWS;
        char win_buf[PATH_MAX];
        const char *win_dir = get_dynamic_windows_home(win_buf, sizeof(win_buf));
        chdir(win_dir);
        char shown[PATH_MAX];
        format_display_path(win_dir, shown, sizeof(shown));
        set_fg((RGB){0x38, 0xbd, 0xf8});
        printf(BOLD "[+] Switched to Windows Native Mode.\n" RESET);
        set_fg((RGB){0x94, 0xa3, 0xb8});
        printf("    Working directory set to Windows User Home (%s)\n\n", shown);
    } else if (strcasecmp(arg, "linux") == 0 || strcasecmp(arg, "unix") == 0) {
        g_os_mode = OS_MODE_LINUX;
        const char *h = getenv("HOME");
        if (h) chdir(h);
        set_fg((RGB){0xa6, 0xe3, 0xa1});
        printf(BOLD "[+] Switched to Linux Unix Mode.\n" RESET);
        set_fg((RGB){0x94, 0xa3, 0xb8});
        printf("    Working directory set to Linux User Home (~)\n\n");
    } else if (strcasecmp(arg, "hybrid") == 0 || strcasecmp(arg, "dual") == 0) {
        g_os_mode = OS_MODE_HYBRID;
        set_fg((RGB){0xc0, 0x84, 0xfc});
        printf(BOLD "[+] Switched to Hybrid Interop Mode (Dual Windows + Linux).\n\n" RESET);
    } else {
        fprintf(stderr, "os: unknown mode '%s'. Use 'win', 'linux', or 'hybrid'.\n", arg);
        return 1;
    }
    return 0;
}

static int bi_fs(void)
{
    printf("\n");
    set_fg((RGB){0xcb, 0xa6, 0xf7});
    printf(BOLD "  ================== DUAL OS FILESYSTEM MAPPING ==================\n" RESET);

    /* Dynamically Scan Active Windows Partitions in /mnt/ */
    set_fg((RGB){0x38, 0xbd, 0xf8});
    printf(BOLD "\n  [DETECTED WINDOWS HOST PARTITIONS]\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("  Drive Letter    WSL Mount Point   Filesystem Type   Status\n");
    printf("  ------------    ---------------   ---------------   ------\n");

    int found_win_drives = 0;
    DIR *mnt_dir = opendir("/mnt");
    if (mnt_dir) {
        struct dirent *ent;
        while ((ent = readdir(mnt_dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (strlen(ent->d_name) == 1 && isalpha((unsigned char)ent->d_name[0])) {
                char d_upper = toupper((unsigned char)ent->d_name[0]);
                char full_mnt[PATH_MAX];
                snprintf(full_mnt, sizeof(full_mnt), "/mnt/%s", ent->d_name);
                set_fg((RGB){0x38, 0xbd, 0xf8}); printf("  %c:\\             ", d_upper);
                set_fg((RGB){0xff, 0xff, 0xff}); printf("%-17s DrvFs / 9p       Active (Accessible)\n", full_mnt);
                found_win_drives++;
            }
        }
        closedir(mnt_dir);
    }
    if (!found_win_drives) {
        set_fg((RGB){0x94, 0xa3, 0xb8});
        printf("  (No /mnt/<drive> partitions detected - Running on Native Linux/macOS)\n");
    }

    /* Linux VFS Section (Real paths) */
    set_fg((RGB){0xa6, 0xe3, 0xa1});
    printf(BOLD "\n  [LINUX UNIX VIRTUAL FILESYSTEMS (VFS)]\n" RESET);
    set_fg((RGB){0x94, 0xa3, 0xb8});
    printf("  Virtual Path    Target Storage    Purpose\n");
    printf("  ------------    --------------    -------\n");
    set_fg((RGB){0xa6, 0xe3, 0xa1}); printf("  /               ");
    set_fg((RGB){0xff, 0xff, 0xff}); printf("rootfs            Unix Root Environment\n");

    const char *h = getenv("HOME");
    set_fg((RGB){0xa6, 0xe3, 0xa1}); printf("  ~ (%s) ", h ? h : "/home/user");
    set_fg((RGB){0xff, 0xff, 0xff}); printf("User Home Directory & Configs\n");

    set_fg((RGB){0xa6, 0xe3, 0xa1}); printf("  /proc           ");
    set_fg((RGB){0xff, 0xff, 0xff}); printf("procfs (RAM)      Kernel Virtual Process Tree & Metrics\n");
    set_fg((RGB){0xa6, 0xe3, 0xa1}); printf("  /sys            ");
    set_fg((RGB){0xff, 0xff, 0xff}); printf("sysfs (RAM)       Device Drivers & Kernel Subsystems\n");

    set_fg((RGB){0x58, 0x5b, 0x70});
    printf("  ─────────────────────────────────────────────────────────────────\n");
    set_fg((RGB){0xfa, 0xb3, 0x87});
    printf("  Navigation Tips:\n");
    printf("  - Switch Windows Drive : type  C:  or  E:  or  D:\n");
    printf("  - Switch Linux Folder  : type  cd ~  or  cd /\n\n" RESET);
    return 0;
}

static int is_drive_letter(const char *name)
{
    if (!name) return 0;
    if (isalpha((unsigned char)name[0]) && name[1] == ':' && name[2] == '\0')
        return 1;
    return 0;
}

static const char *BUILTIN_NAMES[] = {
    "cd", "pwd", "help", "theme", "mode-switch", "sched", "monitor", "clear", "cls", "dir", "type", "md", "rd", "del", "copy", "move", "ren", "os", "mode", "fs", "drives", "export", "unset", "jobs", "history", "user", "welcome", "fastfetch", "fetch", "exit", "quit", NULL
};

static int is_builtin(const char *name)
{
    if (!name) return 0;
    if (is_drive_letter(name)) return 1;
    for (int i = 0; BUILTIN_NAMES[i]; i++)
        if (strcmp(name, BUILTIN_NAMES[i]) == 0) return 1;
    return 0;
}

/* Returns 1 if argv[0] was a built-in (status stored in *status). */
static int run_builtin(int argc, char **argv, int *status)
{
    const char *c = argv[0];
    if (is_drive_letter(c)) {
        char drive = tolower((unsigned char)c[0]);
        char target_path[32];
        snprintf(target_path, sizeof(target_path), "/mnt/%c", drive);
        if (chdir(target_path) != 0) {
            fprintf(stderr, "retroshell: cannot switch to drive %c: (path: %s: %s)\n", toupper((unsigned char)drive), target_path, strerror(errno));
            *status = 1;
        } else {
            *status = 0;
        }
        return 1;
    }
    if      (strcmp(c, "cd") == 0)          *status = bi_cd(argc, argv);
    else if (strcmp(c, "pwd") == 0)         { char b[PATH_MAX]; *status = getcwd(b, sizeof b) ? (puts(b), 0) : 1; }
    else if (strcmp(c, "help") == 0)        *status = bi_help();
    else if (strcmp(c, "theme") == 0)       *status = bi_theme(argc, argv);
    else if (strcmp(c, "mode-switch") == 0) *status = bi_mode_switch();
    else if (strcmp(c, "sched") == 0)       *status = bi_sched(argc, argv);
    else if (strcmp(c, "monitor") == 0)     *status = bi_monitor(argc, argv);
    else if (strcmp(c, "clear") == 0 || strcmp(c, "cls") == 0) { printf("\x1b[H\x1b[2J\x1b[3J"); fflush(stdout); *status = 0; }
    else if (strcmp(c, "dir") == 0) {
        /* Run ls with informative formatting when user types 'dir' */
        char *ls_args[MAX_ARGS];
        ls_args[0] = "ls";
        ls_args[1] = "-lah";
        ls_args[2] = "--color=auto";
        int idx = 3;
        for (int k = 1; k < argc && idx < MAX_ARGS - 1; k++) {
            ls_args[idx++] = argv[k];
        }
        ls_args[idx] = NULL;
        pid_t pid = fork();
        if (pid == 0) {
            execvp("ls", ls_args);
            _exit(1);
        }
        int ws;
        waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "type") == 0) {
        /* CMD 'type' behaves like 'cat' */
        argv[0] = "cat";
        pid_t pid = fork();
        if (pid == 0) {
            execvp("cat", argv);
            _exit(1);
        }
        int ws;
        waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "md") == 0) {
        /* CMD 'md' behaves like 'mkdir -p' */
        char *mk_args[MAX_ARGS];
        mk_args[0] = "mkdir";
        mk_args[1] = "-p";
        int idx = 2;
        for (int k = 1; k < argc && idx < MAX_ARGS - 1; k++) mk_args[idx++] = argv[k];
        mk_args[idx] = NULL;
        pid_t pid = fork();
        if (pid == 0) { execvp("mkdir", mk_args); _exit(1); }
        int ws; waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "rd") == 0) {
        /* CMD 'rd' behaves like 'rmdir' */
        argv[0] = "rmdir";
        pid_t pid = fork();
        if (pid == 0) { execvp("rmdir", argv); _exit(1); }
        int ws; waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "del") == 0) {
        /* CMD 'del' behaves like 'rm -f' */
        char *rm_args[MAX_ARGS];
        rm_args[0] = "rm";
        rm_args[1] = "-f";
        int idx = 2;
        for (int k = 1; k < argc && idx < MAX_ARGS - 1; k++) rm_args[idx++] = argv[k];
        rm_args[idx] = NULL;
        pid_t pid = fork();
        if (pid == 0) { execvp("rm", rm_args); _exit(1); }
        int ws; waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "copy") == 0) {
        /* CMD 'copy' behaves like 'cp -r' */
        char *cp_args[MAX_ARGS];
        cp_args[0] = "cp";
        cp_args[1] = "-r";
        int idx = 2;
        for (int k = 1; k < argc && idx < MAX_ARGS - 1; k++) cp_args[idx++] = argv[k];
        cp_args[idx] = NULL;
        pid_t pid = fork();
        if (pid == 0) { execvp("cp", cp_args); _exit(1); }
        int ws; waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "move") == 0 || strcmp(c, "ren") == 0) {
        /* CMD 'move' / 'ren' behaves like 'mv' */
        argv[0] = "mv";
        pid_t pid = fork();
        if (pid == 0) { execvp("mv", argv); _exit(1); }
        int ws; waitpid(pid, &ws, 0);
        *status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 1;
    }
    else if (strcmp(c, "os") == 0 || strcmp(c, "mode") == 0)   *status = bi_os(argc, argv);
    else if (strcmp(c, "fs") == 0 || strcmp(c, "drives") == 0) *status = bi_fs();
    else if (strcmp(c, "fastfetch") == 0 || strcmp(c, "fetch") == 0) { print_banner(); fflush(stdout); *status = 0; }
    else if (strcmp(c, "export") == 0)      *status = bi_export(argc, argv);
    else if (strcmp(c, "unset") == 0)       *status = bi_unset(argc, argv);
    else if (strcmp(c, "jobs") == 0)        *status = bi_jobs();
    else if (strcmp(c, "history") == 0)     *status = bi_history();
    else if (strcmp(c, "user") == 0)        *status = bi_user(argc, argv);
    else if (strcmp(c, "welcome") == 0)     *status = bi_welcome();
    else if (strcmp(c, "exit") == 0 || strcmp(c, "quit") == 0) {
        int code = argc > 1 ? atoi(argv[1]) : last_status;
        set_fg(AMBER); printf("SYS_HALT: Shutting down shell... Goodbye!\n" RESET);
        exit(code);
    }
    else return 0;
    return 1;
}

static int is_assignment(const char *s)
{
    if (!s) return 0;
    if (!isalpha((unsigned char)*s) && *s != '_') return 0;
    const char *p = s + 1;
    while (*p && *p != '=') {
        if (!isalnum((unsigned char)*p) && *p != '_') return 0;
        p++;
    }
    return (*p == '=');
}

static int count_assignments(const Stage *s)
{
    int k = 0;
    while (k < s->argc && is_assignment(s->argv[k]))
        k++;
    return k;
}

static void apply_assignment(char *arg)
{
    char *eq = strchr(arg, '=');
    if (eq) {
        *eq = '\0';
        setenv(arg, eq + 1, 1);
        *eq = '=';
    }
}

/* ------------------------------------------------------------------
 * Running a pipeline:  cmd1 | cmd2 | cmd3
 *
 *   - make one pipe() between each pair of neighbours
 *   - fork one child per command
 *   - in each child: dup2() the pipe ends onto stdin/stdout, close
 *     every pipe fd, apply < > redirections, then exec (or run a
 *     built-in)
 *   - the parent closes all pipe fds and waits for every child
 *
 * A single built-in (no pipe) runs inside the shell itself so that
 * `cd`, `theme`, `mode-switch`, `exit` can change the shell's state.
 * ------------------------------------------------------------------ */
static int decode_status(int st, int report)
{
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) {
        int s = WTERMSIG(st);
        if (report) {
            if (s == SIGINT) putchar('\n');
            else if (s != SIGPIPE)
                fprintf(stderr, "retroshell: terminated by signal %d (%s)\n", s, strsignal(s));
        }
        return 128 + s;
    }
    return 1;
}

static int run_pipeline(Stage *st, int n, int is_bg, const char *cmd_str)
{
    /* ---- assignments only, e.g. FOO=1 BAR=2 ---- */
    if (n == 1 && count_assignments(&st[0]) == st[0].argc) {
        for (int k = 0; k < st[0].argc; k++)
            apply_assignment(st[0].argv[k]);
        return 0;
    }

    /* ---- one built-in, no pipe, foreground: run in the shell process ---- */
    int num_assign0 = (n == 1) ? count_assignments(&st[0]) : 0;
    if (!is_bg && n == 1 && num_assign0 < st[0].argc && is_builtin(st[0].argv[num_assign0])) {
        int status = 0;
        int save_in = dup(STDIN_FILENO), save_out = dup(STDOUT_FILENO), save_err = dup(STDERR_FILENO);
        fflush(stdout);
        fflush(stderr);
        if (apply_redirs(&st[0]) < 0) status = 1;
        else {
            for (int k = 0; k < num_assign0; k++)
                apply_assignment(st[0].argv[k]);
            run_builtin(st[0].argc - num_assign0, st[0].argv + num_assign0, &status);
        }
        fflush(stdout);
        fflush(stderr);
        dup2(save_in, STDIN_FILENO);   close(save_in);
        dup2(save_out, STDOUT_FILENO); close(save_out);
        dup2(save_err, STDERR_FILENO); close(save_err);
        return status;
    }

    /* ---- create the pipes ---- */
    int pipes[MAX_STAGES][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("retroshell: pipe");
            for (int j = 0; j < i; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            return 1;
        }
    }

    /* ---- fork one child per command ---- */
    pid_t pids[MAX_STAGES];
    int forked = 0, failed = 0;
    fflush(stdout);                          /* don't duplicate buffered output */
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("retroshell: fork"); failed = 1; break; }

        if (pid == 0) {                      /* ---- child ---- */
            if (is_bg) {
                signal(SIGINT, SIG_IGN);     /* Background jobs ignore Ctrl+C */
                signal(SIGQUIT, SIG_IGN);
                /* If background job doesn't have an explicit input file, redirect stdin from /dev/null */
                if (i == 0 && !st[i].in_file) {
                    int devnull = open("/dev/null", O_RDONLY);
                    if (devnull >= 0) {
                        dup2(devnull, STDIN_FILENO);
                        close(devnull);
                    }
                }
            } else {
                signal(SIGINT, SIG_DFL);     /* Ctrl+C must kill foreground child */
                signal(SIGQUIT, SIG_DFL);
            }

            if (i > 0)     dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < n - 1) dup2(pipes[i][1], STDOUT_FILENO);
            for (int j = 0; j < n - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); }

            if (apply_redirs(&st[i]) < 0) _exit(1);   /* files override pipes */

            int num_assign = count_assignments(&st[i]);
            for (int k = 0; k < num_assign; k++)
                apply_assignment(st[i].argv[k]);
            char **cmd_argv = st[i].argv + num_assign;
            int cmd_argc = st[i].argc - num_assign;
            if (cmd_argc == 0) _exit(0);

            int status;
            if (run_builtin(cmd_argc, cmd_argv, &status)) {
                fflush(stdout);
                _exit(status);
            }
            execvp(cmd_argv[0], cmd_argv);
            /* If command not found, try appending Windows .exe or .cmd */
            if (errno == ENOENT && cmd_argv[0]) {
                char win_cmd[PATH_MAX];
                char *orig0 = cmd_argv[0];

                if (!strchr(cmd_argv[0], '.')) {
                    snprintf(win_cmd, sizeof(win_cmd), "%s.exe", orig0);
                    cmd_argv[0] = win_cmd;
                    execvp(win_cmd, cmd_argv);

                    snprintf(win_cmd, sizeof(win_cmd), "%s.cmd", orig0);
                    cmd_argv[0] = win_cmd;
                    execvp(win_cmd, cmd_argv);
                }

                /* If still not found, check standard Windows system directories */
                snprintf(win_cmd, sizeof(win_cmd), "/mnt/c/Windows/System32/%s%s", orig0, strchr(orig0, '.') ? "" : ".exe");
                if (access(win_cmd, X_OK) == 0) {
                    cmd_argv[0] = win_cmd;
                    execvp(win_cmd, cmd_argv);
                }

                snprintf(win_cmd, sizeof(win_cmd), "/mnt/c/Windows/%s%s", orig0, strchr(orig0, '.') ? "" : ".exe");
                if (access(win_cmd, X_OK) == 0) {
                    cmd_argv[0] = win_cmd;
                    execvp(win_cmd, cmd_argv);
                }

                cmd_argv[0] = orig0;
            }
            /* only reached if exec failed */
            if (errno == ENOENT) fprintf(stderr, "retroshell: %s: command not found\n", cmd_argv[0]);
            else                 fprintf(stderr, "retroshell: %s: %s\n", cmd_argv[0], strerror(errno));
            _exit(errno == ENOENT ? 127 : 126);
        }
        pids[forked++] = pid;                /* ---- parent ---- */
    }

    /* parent must close its copies, otherwise readers never see EOF */
    for (int j = 0; j < n - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); }

    /* ---- if background job, register it and do not block ---- */
    if (is_bg) {
        if (forked > 0) {
            add_job(pids[forked - 1], cmd_str);
        }
        return 0;
    }

    /* ---- wait for all children; the exit status is the LAST command's ---- */
    int status = failed ? 1 : 0;
    for (int i = 0; i < forked; i++) {
        int st_wait;
        while (waitpid(pids[i], &st_wait, 0) < 0) {
            if (errno == EINTR) continue;    /* interrupted by Ctrl+C: keep waiting */
            perror("retroshell: waitpid");
            st_wait = 0;
            break;
        }
        if (i == n - 1) status = decode_status(st_wait, 1);
    }
    return status;
}

/* ------------------------------------------------------------------
 * Interactive line reader with history (Up/Down arrow) & tab completion
 * ------------------------------------------------------------------ */
static void handle_tab_completion(char *buf, int *len_ptr, int *pos_ptr, int max_len)
{
    int pos = *pos_ptr;
    int start = pos;
    while (start > 0 && !isspace((unsigned char)buf[start - 1])) {
        start--;
    }
    int wlen = pos - start;
    char prefix[256];
    if (wlen >= (int)sizeof(prefix)) return;
    memcpy(prefix, buf + start, wlen);
    prefix[wlen] = '\0';

    char dir_path[256] = ".";
    const char *file_prefix = prefix;
    char *slash = strrchr(prefix, '/');
    if (slash) {
        size_t dlen = (size_t)(slash - prefix);
        if (dlen == 0) {
            strcpy(dir_path, "/");
        } else {
            memcpy(dir_path, prefix, dlen);
            dir_path[dlen] = '\0';
        }
        file_prefix = slash + 1;
    }

    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *de;
    char *matches[128];
    int nmatches = 0;
    size_t fplen = strlen(file_prefix);

    while ((de = readdir(d)) != NULL && nmatches < 128) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        if (strncmp(de->d_name, file_prefix, fplen) == 0) {
            matches[nmatches++] = strdup(de->d_name);
        }
    }
    closedir(d);

    if (nmatches == 0) return;

    if (nmatches == 1) {
        const char *match = matches[0];
        const char *rest = match + fplen;
        size_t rlen = strlen(rest);

        char full[512];
        if (strcmp(dir_path, ".") == 0) snprintf(full, sizeof(full), "%s", match);
        else if (strcmp(dir_path, "/") == 0) snprintf(full, sizeof(full), "/%s", match);
        else snprintf(full, sizeof(full), "%s/%s", dir_path, match);

        struct stat st;
        int is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
        char suffix = is_dir ? '/' : ' ';

        if (*len_ptr + (int)rlen + 1 < max_len) {
            int shift = (int)rlen + 1;
            memmove(buf + pos + shift, buf + pos, *len_ptr - pos + 1);
            memcpy(buf + pos, rest, rlen);
            buf[pos + rlen] = suffix;
            *len_ptr += shift;
            pos += shift;
            *pos_ptr = pos;
            write(STDOUT_FILENO, buf + pos - shift, *len_ptr - (pos - shift));
            for (int k = 0; k < *len_ptr - pos; k++) write(STDOUT_FILENO, "\b", 1);
        }
    } else {
        size_t clen = strlen(matches[0]);
        for (int m = 1; m < nmatches; m++) {
            size_t k = 0;
            while (k < clen && matches[m][k] && matches[0][k] == matches[m][k]) k++;
            clen = k;
        }
        if (clen > fplen) {
            size_t add = clen - fplen;
            if (*len_ptr + (int)add < max_len) {
                memmove(buf + pos + add, buf + pos, *len_ptr - pos + 1);
                memcpy(buf + pos, matches[0] + fplen, add);
                *len_ptr += add;
                pos += add;
                *pos_ptr = pos;
                write(STDOUT_FILENO, buf + pos - add, *len_ptr - (pos - add));
                for (int k = 0; k < *len_ptr - pos; k++) write(STDOUT_FILENO, "\b", 1);
            }
        } else {
            printf("\n");
            for (int m = 0; m < nmatches; m++) {
                printf("%s  ", matches[m]);
            }
            printf("\n");
            print_prompt();
            write(STDOUT_FILENO, buf, *len_ptr);
            for (int k = 0; k < *len_ptr - pos; k++) write(STDOUT_FILENO, "\b", 1);
        }
    }

    for (int m = 0; m < nmatches; m++) free(matches[m]);
}

static ssize_t read_input_line(char **line_out)
{
    if (!isatty(STDIN_FILENO)) {
        char *line = NULL;
        size_t cap = 0;
        errno = 0;
        ssize_t len = getline(&line, &cap, stdin);
        if (len < 0) { free(line); *line_out = NULL; return -1; }
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len > 0 && (line[len - 1] == '\r')) line[--len] = '\0';
        *line_out = line;
        return len;
    }

    struct termios orig, raw;
    if (tcgetattr(STDIN_FILENO, &orig) < 0) {
        char *line = NULL;
        size_t cap = 0;
        ssize_t len = getline(&line, &cap, stdin);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        *line_out = line;
        return len;
    }
    raw = orig;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    char buf[4096];
    int len = 0;
    int pos = 0;
    buf[0] = '\0';

    int hist_idx = cmd_hist_count;
    char saved_curr[4096];
    saved_curr[0] = '\0';

    for (;;) {
        char c;
        ssize_t nr = read(STDIN_FILENO, &c, 1);
        if (nr <= 0) {
            if (nr < 0 && errno == EINTR) continue;
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            *line_out = NULL;
            return -1;
        }

        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            buf[len] = '\0';
            if (len > 0) hist_add(buf);
            *line_out = strdup(buf);
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            return len;
        }

        if (c == 3) { /* Ctrl+C */
            write(STDOUT_FILENO, "^C\n", 3);
            *line_out = strdup("");
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            return 0;
        }

        if (c == 4) { /* Ctrl+D */
            if (len == 0) {
                tcsetattr(STDIN_FILENO, TCSANOW, &orig);
                *line_out = NULL;
                return -1;
            }
            continue;
        }

        if (c == 12) { /* Ctrl+L: clear screen */
            write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
            print_prompt();
            write(STDOUT_FILENO, buf, len);
            for (int k = 0; k < len - pos; k++) write(STDOUT_FILENO, "\b", 1);
            continue;
        }

        if (c == 127 || c == 8) { /* Backspace */
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos + 1);
                pos--;
                len--;
                write(STDOUT_FILENO, "\b", 1);
                write(STDOUT_FILENO, buf + pos, len - pos);
                write(STDOUT_FILENO, " ", 1);
                for (int k = 0; k <= len - pos; k++) write(STDOUT_FILENO, "\b", 1);
            }
            continue;
        }

        if (c == '\t') { /* Tab completion */
            handle_tab_completion(buf, &len, &pos, (int)sizeof(buf));
            continue;
        }

        if (c == '\x1b') { /* Escape sequence */
            char seq[4] = {0};
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (seq[0] == '[' || seq[0] == 'O') {
                if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
                if (seq[1] == 'A') { /* UP ARROW */
                    if (hist_idx > 0) {
                        if (hist_idx == cmd_hist_count) {
                            memcpy(saved_curr, buf, len + 1);
                        }
                        hist_idx--;
                        while (pos > 0) { write(STDOUT_FILENO, "\b", 1); pos--; }
                        for (int k = 0; k < len; k++) write(STDOUT_FILENO, " ", 1);
                        for (int k = 0; k < len; k++) write(STDOUT_FILENO, "\b", 1);
                        len = (int)strlen(cmd_history[hist_idx]);
                        if (len >= (int)sizeof(buf) - 1) len = sizeof(buf) - 2;
                        memcpy(buf, cmd_history[hist_idx], len);
                        buf[len] = '\0';
                        pos = len;
                        write(STDOUT_FILENO, buf, len);
                    }
                } else if (seq[1] == 'B') { /* DOWN ARROW */
                    if (hist_idx < cmd_hist_count) {
                        hist_idx++;
                        const char *src = (hist_idx == cmd_hist_count) ? saved_curr : cmd_history[hist_idx];
                        while (pos > 0) { write(STDOUT_FILENO, "\b", 1); pos--; }
                        for (int k = 0; k < len; k++) write(STDOUT_FILENO, " ", 1);
                        for (int k = 0; k < len; k++) write(STDOUT_FILENO, "\b", 1);
                        len = (int)strlen(src);
                        if (len >= (int)sizeof(buf) - 1) len = sizeof(buf) - 2;
                        memcpy(buf, src, len);
                        buf[len] = '\0';
                        pos = len;
                        write(STDOUT_FILENO, buf, len);
                    }
                } else if (seq[1] == 'C') { /* RIGHT ARROW */
                    if (pos < len) {
                        write(STDOUT_FILENO, &buf[pos], 1);
                        pos++;
                    }
                } else if (seq[1] == 'D') { /* LEFT ARROW */
                    if (pos > 0) {
                        write(STDOUT_FILENO, "\b", 1);
                        pos--;
                    }
                } else if (seq[1] == 'H') { /* HOME */
                    while (pos > 0) { write(STDOUT_FILENO, "\b", 1); pos--; }
                } else if (seq[1] == 'F') { /* END */
                    while (pos < len) { write(STDOUT_FILENO, &buf[pos], 1); pos++; }
                } else if (seq[1] >= '1' && seq[1] <= '6') {
                    char seq2;
                    if (read(STDIN_FILENO, &seq2, 1) > 0 && seq2 == '~') {
                        if (seq[1] == '1' || seq[1] == '7') { /* HOME */
                            while (pos > 0) { write(STDOUT_FILENO, "\b", 1); pos--; }
                        } else if (seq[1] == '4' || seq[1] == '8') { /* END */
                            while (pos < len) { write(STDOUT_FILENO, &buf[pos], 1); pos++; }
                        } else if (seq[1] == '3') { /* DELETE */
                            if (pos < len) {
                                memmove(buf + pos, buf + pos + 1, len - pos);
                                len--;
                                write(STDOUT_FILENO, buf + pos, len - pos);
                                write(STDOUT_FILENO, " ", 1);
                                for (int k = 0; k <= len - pos; k++) write(STDOUT_FILENO, "\b", 1);
                            }
                        }
                    }
                }
            }
            continue;
        }

        /* Printable characters */
        if ((unsigned char)c >= 32 && len < (int)sizeof(buf) - 2) {
            if (pos == len) {
                buf[len++] = c;
                buf[len] = '\0';
                pos++;
                write(STDOUT_FILENO, &c, 1);
            } else {
                memmove(buf + pos + 1, buf + pos, len - pos + 1);
                buf[pos++] = c;
                len++;
                write(STDOUT_FILENO, buf + pos - 1, len - pos + 1);
                for (int k = 0; k < len - pos; k++) write(STDOUT_FILENO, "\b", 1);
            }
        }
    }
}

/* ------------------------------------------------------------------
 * main: read - parse - execute loop
 * ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--ascii") == 0) use_powerline = 0;

    /* Handle Ctrl+C ourselves (no SA_RESTART => getline() returns EINTR) */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGQUIT, SIG_IGN);

    load_or_ask_username();
    select_os_boot_mode();
    print_banner();

    for (;;) {
        reap_jobs();
        print_prompt();

        char *line = NULL;
        ssize_t len = read_input_line(&line);
        if (len < 0) {
            printf("\nlogout\n");
            free(line);
            break;
        }
        if (len == 0 && (!line || line[0] == '\0')) {
            free(line);
            continue;
        }

        LineSegment segs[MAX_COMMANDS];
        int num_segs = split_line_segments(line, segs, MAX_COMMANDS);
        if (num_segs <= 0) {
            free(line);
            continue;
        }

        int run_cmd = 1;
        for (int c = 0; c < num_segs; c++) {
            if (c > 0) {
                ChainOp prev_op = segs[c - 1].next_op;
                if (prev_op == OP_SEQ || prev_op == OP_BG) {
                    run_cmd = 1;
                } else if (prev_op == OP_AND) {
                    run_cmd = (last_status == 0);
                } else if (prev_op == OP_OR) {
                    run_cmd = (last_status != 0);
                }
            }

            if (!run_cmd) {
                free(segs[c].cmd_str);
                continue;
            }

            /* Tokenize and expand variables JUST-IN-TIME right before execution */
            size_t seg_buf_sz = 16384 + strlen(segs[c].cmd_str) * 4;
            char *seg_buf = malloc(seg_buf_sz);
            if (!seg_buf) {
                free(segs[c].cmd_str);
                continue;
            }

            char *tok[MAX_ARGS];
            int   isop[MAX_ARGS];
            int   isglob[MAX_ARGS];
            int n = tokenize(segs[c].cmd_str, seg_buf, seg_buf_sz, tok, isop, isglob, MAX_ARGS);
            if (n <= 0) {
                pool_free_all();
                free(seg_buf);
                free(segs[c].cmd_str);
                continue;
            }

            Stage st[MAX_STAGES];
            int nst = 0;
            if (build_pipeline(tok, isop, isglob, n, st, &nst) < 0) {
                last_status = 2;
                pool_free_all();
                free(seg_buf);
                free(segs[c].cmd_str);
                break;
            }

            char cmd_str[128];
            format_cmd_string(tok, 0, n, cmd_str, sizeof(cmd_str));

            last_status = run_pipeline(st, nst, segs[c].is_bg, cmd_str);

            pool_free_all();
            free(seg_buf);
            free(segs[c].cmd_str);
        }
        free(line);
    }
    return last_status;
}
