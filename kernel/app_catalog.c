#include "app_catalog.h"

static const char *const terminal_commands[] = {"terminal", "term", "console", 0};
static const char *const files_commands[] = {"files", "file", "computer", "explorer", 0};
static const char *const programs_commands[] = {"programs", "apps", 0};
static const char *const about_commands[] = {"about", "help", 0};
static const char *const settings_commands[] = {"settings", "control panel", 0};
static const char *const ide_commands[] = {"ide", "editor", 0};

static const asteros_app_info_t app_catalog[] = {
    {ASTEROS_APP_TERMINAL, "Terminal",
     "Interactive command shell with basic text commands.",
     "Try: help, apps, echo hello, history, info.",
     "Run names: terminal, term, console",
     "Limits: no process loader; pwd has no directory service.",
     terminal_commands},
    {ASTEROS_APP_FILES, "Files",
     "Browse and open files in the session RAM filesystem.",
     "Up/Down selects; Enter opens the selected file in IDE.",
     "Run names: files, file, computer, explorer",
     "Limits: 8 slots, 1,023 bytes each; no disk mount.",
     files_commands},
    {ASTEROS_APP_PROGRAMS, "Programs",
     "Inventory of the six built-in desktop applications.",
     "Use Start or Run to open a built-in application.",
     "Run names: programs, apps",
     "Limits: inventory only; no transient-program loader.",
     programs_commands},
    {ASTEROS_APP_ABOUT, "About",
     "AsterOS identity and keyboard/desktop usage guidance.",
     "W/S or arrows navigate; Enter opens; Esc closes.",
     "Run names: about, help",
     "Limits: experimental i386 protected-mode text UI.",
     about_commands},
    {ASTEROS_APP_SETTINGS, "Settings",
     "Adjust pointer visibility, speed, shape, and color.",
     "V visibility; S speed; C shape; K color; B hover-click.",
     "Run names: settings, control panel",
     "Limits: pointer preferences reset on reboot.",
     settings_commands},
    {ASTEROS_APP_IDE, "IDE",
     "Edit TinyLang source; F5 checks syntax and labels.",
     "Ctrl+S save; Ctrl+Alt+S Save As; Ctrl+O open.",
     "Run names: ide, editor",
     "Limits: F5 does not compile; files are RAM-only.",
     ide_commands}
};

static char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z') return (char)(value - ('A' - 'a'));
    return value;
}
static uint8_t command_equals(const char *command, const char *expected) {
    while (*command == ' ' || *command == '\t') command++;
    while (*expected) {
        if (!*command || ascii_lower(*command) != *expected) return 0u;
        command++;
        expected++;
    }
    while (*command == ' ' || *command == '\t') command++;
    return (uint8_t)(*command == '\0');
}
uint8_t asteros_app_catalog_count(void) {
    return (uint8_t)(sizeof app_catalog / sizeof app_catalog[0]);
}
const asteros_app_info_t *asteros_app_catalog_at(uint8_t index) {
    return index < asteros_app_catalog_count() ? &app_catalog[index] : 0;
}
const asteros_app_info_t *asteros_app_catalog_by_id(int8_t id) {
    uint8_t index;
    for (index = 0; index < asteros_app_catalog_count(); index++)
        if (app_catalog[index].id == id) return &app_catalog[index];
    return 0;
}
const asteros_app_info_t *asteros_app_catalog_by_command(const char *command) {
    uint8_t index;
    if (!command) return 0;
    for (index = 0; index < asteros_app_catalog_count(); index++) {
        const char *const *alias = app_catalog[index].commands;
        while (*alias) {
            if (command_equals(command, *alias)) return &app_catalog[index];
            alias++;
        }
    }
    return 0;
}
int8_t asteros_app_catalog_resolve_command(const char *command) {
    const asteros_app_info_t *info = asteros_app_catalog_by_command(command);
    return info ? info->id : ASTEROS_HIT_NONE;
}
