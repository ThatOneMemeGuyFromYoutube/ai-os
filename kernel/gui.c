#include "gui.h"
#include "terminal.h"
#include "desktop_input.h"
#include "keyboard.h"
#include "editor.h"
#include "fs.h"
#include "ide_file.h"
#include "tinylang_check.h"
#include "app_catalog.h"
#ifdef ASTEROS_GUI_TEST
volatile uint16_t asteros_gui_test_vga[80u * 25u];
#define VGA asteros_gui_test_vga
#else
#define VGA ((volatile uint16_t*)0xB8000)
#endif
#define WIDTH 80
#define HEIGHT 25
#define ATTR_NORMAL 0x07
#define ATTR_TITLE 0x1F
#define ATTR_PANEL 0x70
#define ATTR_STATUS 0x1E
#define ATTR_SELECT 0x71
#define ATTR_DESKTOP 0x1B
#define ATTR_ICON 0x1F
#define ATTR_TASK 0x70
#define ATTR_TASK_ACTIVE 0x71
#define TERM_X 22
#define TERM_W 54
#define TERM_INPUT_MAX ASTEROS_TERMINAL_LINE_MAX
#define EDITOR_TEXT_X 27u
#define EDITOR_TEXT_WIDTH 48u
#define EDITOR_VIEW_ROWS 10u
#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_TAB 0x09

static uint8_t selected;
static uint8_t terminal_result;
static int8_t terminal_help_app;
static uint8_t terminal_help_requested;
static uint8_t terminal_length;
static int8_t active_app;
static int16_t mouse_x;
static int16_t mouse_y;
static uint8_t cursor_active;
static uint16_t cursor_saved;
static uint8_t cursor_visible;
static uint8_t cursor_speed;
static uint8_t cursor_shape;
static uint8_t cursor_color;
static uint8_t keyboard_click_enabled;
static uint8_t context_menu_open;
static uint8_t context_menu_selection;
static uint8_t start_menu_open;
static uint8_t start_menu_selection;
static uint8_t run_dialog_open;
static uint8_t run_length;
static char run_buffer[TERM_INPUT_MAX + 1];
static char run_status[TERM_INPUT_MAX + 1];
static uint8_t context_menu_x;
static uint8_t context_menu_y;
static int8_t context_menu_target;
static const char cursor_glyphs[] = {'*', '+', 'X', '>'};
static const char *cursor_shape_names[] = {"Star", "Plus", "Cross", "Arrow"};
static const uint8_t cursor_attributes[] = {0x0F, 0x0E, 0x0B, 0x0C, 0x0D};
static const char *cursor_color_names[] = {"White", "Yellow", "Cyan", "Red", "Magenta"};
static char terminal_buffer[TERM_INPUT_MAX + 1];
static char terminal_output[TERM_INPUT_MAX + 1];
static asteros_terminal_history_t terminal_history_state;
static char terminal_history_output[TERM_INPUT_MAX + 1];
static asteros_editor_t editor_state;
static uint16_t editor_scroll_line;
static asteros_ide_file_t editor_file;
static char editor_prompt_name[ASTEROS_FS_NAME_MAX + 1u];
static uint8_t editor_prompt_length;
static uint8_t editor_prompt_mode;
static char editor_status[TERM_INPUT_MAX + 1u];
static char files_status[TERM_INPUT_MAX + 1u];
static uint8_t files_selected;
static uint8_t ata_disk_present;
static uint8_t asteros_volume_present;
static const char editor_default_source[] =
    "# TinyLang source editor\nmov eax, 42\nadd eax, 1\nexit";
static const char *items[] = {"Terminal", "Files", "Programs", "About", "Settings", "IDE"};
static const char *start_menu_items[] = {
    "Terminal", "Files", "Programs", "IDE", "Settings", "About", "Run..."
};
static const uint8_t start_menu_apps[] = {
    ASTEROS_APP_TERMINAL, ASTEROS_APP_FILES, ASTEROS_APP_PROGRAMS,
    ASTEROS_APP_IDE, ASTEROS_APP_SETTINGS, ASTEROS_APP_ABOUT
};

enum { TERM_READY = 0, TERM_HELP, TERM_APPS, TERM_INFO, TERM_VER, TERM_ECHO, TERM_PWD, TERM_HISTORY, TERM_UNAME, TERM_UNKNOWN };

static void cell(uint8_t x,uint8_t y,char c,uint8_t a){if(x<WIDTH&&y<HEIGHT)VGA[(uint16_t)y*WIDTH+x]=((uint16_t)a<<8)|(uint8_t)c;}
static uint16_t cursor_index(void){
    return (uint16_t)(mouse_y / 4) * WIDTH + (uint16_t)(mouse_x / 4);
}
static void cursor_clear(void){
    if(cursor_active){
        VGA[cursor_index()]=cursor_saved;
        cursor_active=0;
    }
}
static void cursor_draw(void){
    uint16_t index;
    if(!cursor_visible||mouse_x<0||mouse_y<0||mouse_x>=WIDTH*4||mouse_y>=HEIGHT*4)return;
    index=cursor_index();
    cursor_saved=VGA[index];
    VGA[index]=((uint16_t)cursor_attributes[cursor_color]<<8)|(uint8_t)cursor_glyphs[cursor_shape];
    cursor_active=1;
}
static void cursor_setting_change(uint8_t setting){
    if(setting==0)cursor_visible=(uint8_t)!cursor_visible;
    else if(setting==1)cursor_speed=(uint8_t)(cursor_speed%3+1);
    else if(setting==2)cursor_shape=(uint8_t)((cursor_shape+1)%4);
    else if(setting==3)cursor_color=(uint8_t)((cursor_color+1)%5);
}
static void fill(uint8_t x,uint8_t y,uint8_t w,uint8_t h,char c,uint8_t a){for(uint8_t r=0;r<h;r++)for(uint8_t col=0;col<w;col++)cell((uint8_t)(x+col),(uint8_t)(y+r),c,a);}
static void text(uint8_t x,uint8_t y,const char*s,uint8_t a){while(*s&&x<WIDTH)cell(x++,y,*s++,a);}
static void text_n(uint8_t x,uint8_t y,const char*s,uint8_t a,uint8_t n){while(*s&&n--&&x<WIDTH)cell(x++,y,*s++,a);}
static void border(uint8_t x,uint8_t y,uint8_t w,uint8_t h){for(uint8_t i=0;i<w;i++){char edge=(i==0||i==w-1)?'+':'-';cell((uint8_t)(x+i),y,edge,ATTR_PANEL);cell((uint8_t)(x+i),(uint8_t)(y+h-1),edge,ATTR_PANEL);}for(uint8_t i=1;i+1<h;i++){cell(x,(uint8_t)(y+i),'|',ATTR_PANEL);cell((uint8_t)(x+w-1),(uint8_t)(y+i),'|',ATTR_PANEL);}}
static void copy_text(char *destination,const char *source){uint8_t i=0;while(source[i]&&i<TERM_INPUT_MAX){destination[i]=source[i];i++;}destination[i]='\0';}
static void trim_leading_spaces(char **value){while(**value==' '||**value=='\t')(*value)++;}
static void terminal_reset_input(void){terminal_length=0;terminal_buffer[0]='\0';}
static void terminal_reset_view(void){terminal_result=TERM_READY;terminal_output[0]='\0';terminal_history_output[0]='\0';}
static void close_active_app(void){
    if(asteros_app_close_discards_terminal_state(active_app)){
        terminal_reset_input();
        terminal_reset_view();
    }
    active_app=-1;
}
static void terminal_execute(void){
    char *command=terminal_buffer;
    const char *arguments;
    asteros_command_t parsed;
    uint8_t repeat_requested=0;
    uint8_t repeat_succeeded=0;

    terminal_output[0]='\0';
    terminal_history_output[0]='\0';
    terminal_help_app=ASTEROS_HIT_NONE;
    terminal_help_requested=0;
    trim_leading_spaces(&command);
    parsed=terminal_parse_command(command,&arguments);

    if(parsed==ASTEROS_COMMAND_REPEAT){
        repeat_requested=1;
        if(terminal_history_repeat(&terminal_history_state,terminal_buffer,
                                  sizeof terminal_buffer)){
            command=terminal_buffer;
            parsed=terminal_parse_command(command,&arguments);
            repeat_succeeded=1;
        }else{
            /* Keep the previous command on an empty or undersized history. */
            parsed=ASTEROS_COMMAND_UNKNOWN;
        }
    }

    if(parsed==ASTEROS_COMMAND_HISTORY_CLEAR){
        terminal_history_clear(&terminal_history_state);
        terminal_history_output[0]='\0';
        terminal_result=TERM_HISTORY;
        terminal_reset_input();
        return;
    }
    if(parsed==ASTEROS_COMMAND_HISTORY){
        copy_text(terminal_history_output,
                  terminal_history_last(&terminal_history_state));
        terminal_result=TERM_HISTORY;
        terminal_reset_input();
        return;
    }

    /* A failed !! must never become the command that the next !! repeats. */
    if(*command&&(!repeat_requested||repeat_succeeded))
        terminal_history_record(&terminal_history_state,command);

    switch(parsed){
        case ASTEROS_COMMAND_HELP:
            if(*arguments){
                terminal_help_requested=1;
                terminal_help_app=asteros_app_resolve_command(arguments);
            }
            terminal_result=TERM_HELP;
            break;
        case ASTEROS_COMMAND_CLEAR: terminal_result=TERM_READY; break;
        case ASTEROS_COMMAND_APPS: terminal_result=TERM_APPS; break;
        case ASTEROS_COMMAND_INFO: terminal_result=TERM_INFO; break;
        case ASTEROS_COMMAND_VERSION: terminal_result=TERM_VER; break;
        case ASTEROS_COMMAND_UNAME: terminal_result=TERM_UNAME; break;
        case ASTEROS_COMMAND_PWD: terminal_result=TERM_PWD; break;
        case ASTEROS_COMMAND_ECHO:
            copy_text(terminal_output,arguments);
            terminal_result=TERM_ECHO;
            break;
        case ASTEROS_COMMAND_MENU:
            active_app=-1;
            terminal_result=TERM_READY;
            break;
        case ASTEROS_COMMAND_EMPTY: terminal_result=TERM_READY; break;
        default: terminal_result=TERM_UNKNOWN; break;
    }
    terminal_reset_input();
}
static void draw_terminal_result(void){
    switch(terminal_result){
        case TERM_HELP:
            if(terminal_help_requested){
                const asteros_app_info_t *info=asteros_app_catalog_by_id(terminal_help_app);
                if(info){
                    text_n(22,9,info->name,ATTR_STATUS,57);
                    text_n(22,10,info->summary,ATTR_NORMAL,57);
                    text_n(22,11,info->usage,ATTR_NORMAL,57);
                    text_n(22,12,info->aliases,ATTR_NORMAL,57);
                    text_n(22,13,info->limitations,ATTR_STATUS,57);
                }else{
                    text(22,9,"Unknown application topic.",ATTR_STATUS);
                    text(22,11,"Try: help terminal, help files, help ide.",ATTR_NORMAL);
                    text(22,12,"Type apps to list every built-in application.",ATTR_NORMAL);
                }
            }else{
                text(22,9,"help [app] shows application purpose and limits.",ATTR_NORMAL);
                text(22,10,"Try: help terminal, help files, help programs.",ATTR_NORMAL);
                text(22,11,"Also: help about, help settings, help ide.",ATTR_NORMAL);
                text(22,12,"apps lists the same catalog as the Programs window.",ATTR_NORMAL);
                text(22,14,"Commands: clear, echo, info, ver, uname, pwd.",ATTR_NORMAL);
                text(22,15,"history, history clear, !!, menu/exit.",ATTR_NORMAL);
                text(22,17,"No transient-program loader is available yet.",ATTR_STATUS);
            }
            break;
        case TERM_APPS: {
            uint8_t index;
            text(22,8,"BUILT-IN APPLICATIONS",ATTR_STATUS);
            for(index=0;index<asteros_app_catalog_count();index++){
                const asteros_app_info_t *info=asteros_app_catalog_at(index);
                uint8_t y=(uint8_t)(9u+index);
                if(!info)continue;
                text_n(22,y,info->name,ATTR_NORMAL,11);
                text_n(34,y,info->summary,ATTR_NORMAL,44);
            }
            text(22,17,"Type help <app> for usage, aliases and limitations.",ATTR_STATUS);
            break;
        }
        case TERM_INFO:
            text(22,9,"AsterOS 0.1 - 32-bit x86 protected mode",ATTR_NORMAL);
            text(22,10,"Console: VGA text buffer 80x25",ATTR_NORMAL);
            break;
        case TERM_VER:
            text(22,9,"AsterOS 0.1",ATTR_NORMAL);
            break;
        case TERM_UNAME:
            text(22,9,"AsterOS i386 protected-mode kernel",ATTR_NORMAL);
            break;
        case TERM_ECHO:
            text_n(22,9,terminal_output,ATTR_NORMAL,TERM_INPUT_MAX);
            break;
        case TERM_PWD:
            text(22,9,"/",ATTR_NORMAL);
            break;
        case TERM_HISTORY:
            if(terminal_history_output[0]){text(22,9,"last: ",ATTR_STATUS);text_n(28,9,terminal_history_output,ATTR_NORMAL,TERM_INPUT_MAX-6);}else{text(22,9,"No previous command.",ATTR_NORMAL);}
            break;
        case TERM_UNKNOWN:
            text(22,9,"Unknown command. Type help.",ATTR_NORMAL);
            break;
        default:
            break;
    }
}
static void draw_terminal(void){
    text(22,5,"AsterOS terminal",ATTR_STATUS);
    text(22,6,"Type help for commands.",ATTR_NORMAL);
    text(22,8,"aster> ",ATTR_STATUS);
    text_n(29,8,terminal_buffer,ATTR_NORMAL,TERM_INPUT_MAX);
    draw_terminal_result();
    text(22,19,"RAM files available; no process loader yet.",ATTR_STATUS);
}
enum { EDITOR_PROMPT_NONE = 0, EDITOR_PROMPT_SAVE_AS, EDITOR_PROMPT_OPEN };

static void activate_app(uint8_t app);

static void set_short_status(char *destination, const char *message) {
    uint8_t index = 0;
    while (message[index] && index < TERM_INPUT_MAX) {
        destination[index] = message[index];
        index++;
    }
    destination[index] = '\0';
}

static void copy_ide_filename(char *destination, const char *source) {
    uint8_t index = 0;
    while (source[index] && index < ASTEROS_FS_NAME_MAX) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static uint8_t editor_filename_character(char value) {
    return (uint8_t)(
        (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') ||
        value == '_' || value == '-' || value == '.');
}

static uint8_t editor_save_named_file(const char *name) {
    asteros_fs_status_t fs_error;
    asteros_ide_file_status_t status =
        asteros_ide_file_save(&editor_file, &editor_state, name, &fs_error);
    if (status != ASTEROS_IDE_FILE_OK) {
        set_short_status(editor_status,
            asteros_ide_file_status_message(status, fs_error));
        return 0;
    }
    set_short_status(editor_status, "Saved in RAM; files are lost on reboot");
    set_short_status(files_status, "RAM file saved");
    return 1;
}

static uint8_t editor_load_named_file(const char *name) {
    asteros_fs_status_t fs_error;
    asteros_ide_file_status_t status =
        asteros_ide_file_open(&editor_file, &editor_state, name, &fs_error);
    if (status != ASTEROS_IDE_FILE_OK) {
        set_short_status(editor_status,
            asteros_ide_file_status_message(status, fs_error));
        return 0;
    }
    editor_scroll_line = 0;
    set_short_status(editor_status, "Loaded from volatile RAM filesystem");
    return 1;
}

static void editor_begin_prompt(uint8_t mode) {
    if (mode == EDITOR_PROMPT_OPEN && asteros_editor_is_dirty(&editor_state)) {
        set_short_status(editor_status, "Unsaved edits: Ctrl+S saves before opening");
        return;
    }
    editor_prompt_mode = mode;
    copy_ide_filename(editor_prompt_name, editor_file.name);
    editor_prompt_length = 0;
    while (editor_prompt_name[editor_prompt_length] &&
           editor_prompt_length < ASTEROS_FS_NAME_MAX)
        editor_prompt_length++;
    editor_status[0] = '\0';
}

static void editor_finish_prompt(void) {
    uint8_t success;
    editor_prompt_name[editor_prompt_length] = '\0';
    if (editor_prompt_mode == EDITOR_PROMPT_SAVE_AS)
        success = editor_save_named_file(editor_prompt_name);
    else if (editor_prompt_mode == EDITOR_PROMPT_OPEN)
        success = editor_load_named_file(editor_prompt_name);
    else
        success = 0;
    if (success) editor_prompt_mode = EDITOR_PROMPT_NONE;
}

static void editor_check_source(void) {
    asteros_tinylang_diagnostic_t diagnostic;
    asteros_tinylang_status_t status = asteros_tinylang_check(
        asteros_editor_text(&editor_state), editor_state.length, &diagnostic);

    if (status == ASTEROS_TINYLANG_OK) {
        set_short_status(editor_status,
            "Syntax/labels OK; F5 does not build code");
        return;
    }

    {
        char message[TERM_INPUT_MAX + 1u];
        char digits[5];
        uint8_t position = 0;
        uint8_t digit_count = 0;
        uint16_t line = diagnostic.line;
        const char *detail = asteros_tinylang_status_message(status);
        uint8_t index;

        if (line) {
            message[position++] = 'L';
            message[position++] = 'i';
            message[position++] = 'n';
            message[position++] = 'e';
            message[position++] = ' ';
            do {
                digits[digit_count++] = (char)('0' + (line % 10u));
                line = (uint16_t)(line / 10u);
            } while (line && digit_count < sizeof digits);
            while (digit_count && position < TERM_INPUT_MAX)
                message[position++] = digits[--digit_count];
            if (position < TERM_INPUT_MAX) message[position++] = ':';
            if (position < TERM_INPUT_MAX) message[position++] = ' ';
        }

        for (index = 0; detail[index] && position < TERM_INPUT_MAX; index++)
            message[position++] = detail[index];
        message[position] = '\0';
        set_short_status(editor_status, message);

        if (diagnostic.line) {
            uint16_t target_line = (uint16_t)(diagnostic.line - 1u);
            asteros_editor_move_to_line(&editor_state, target_line);
            editor_scroll_line = target_line;
        }
    }
}

static void files_open_selected(void) {
    asteros_fs_info_t info;
    if (asteros_fs_stat(files_selected, &info) != ASTEROS_FS_OK) {
        set_short_status(files_status, "No file selected");
        return;
    }
    if (asteros_editor_is_dirty(&editor_state)) {
        set_short_status(files_status, "IDE has unsaved edits; save before opening");
        return;
    }
    if (editor_load_named_file(info.name)) {
        set_short_status(files_status, "Opened file in IDE");
        activate_app(ASTEROS_APP_IDE);
    } else {
        set_short_status(files_status, editor_status);
    }
}

static void draw_editor(void){
    uint16_t cursor_line=asteros_editor_cursor_line(&editor_state);
    uint16_t cursor_column=asteros_editor_cursor_column(&editor_state);
    uint16_t line_count=asteros_editor_line_count(&editor_state);
    uint16_t row;
    uint16_t scroll_column=cursor_column>=EDITOR_TEXT_WIDTH
        ? (uint16_t)(cursor_column-EDITOR_TEXT_WIDTH+1u) : 0u;
    const char *source=asteros_editor_text(&editor_state);

    if(cursor_line<editor_scroll_line)editor_scroll_line=cursor_line;
    else if(cursor_line>=editor_scroll_line+EDITOR_VIEW_ROWS)
        editor_scroll_line=(uint16_t)(cursor_line-EDITOR_VIEW_ROWS+1u);

    text(22,5,"TinyLang source editor",ATTR_STATUS);
    text(22,6,asteros_editor_is_dirty(&editor_state)?"Modified":"Saved",ATTR_STATUS);
    text(32,6,"File:",ATTR_NORMAL);
    text_n(38,6,editor_file.name,ATTR_STATUS,ASTEROS_FS_NAME_MAX);
    text(22,7,"F5 Check | Ctrl+S Save | Ctrl+Alt+S Save As",ATTR_NORMAL);

    for(row=0;row<EDITOR_VIEW_ROWS;row++){
        uint16_t line=(uint16_t)(editor_scroll_line+row);
        uint8_t y=(uint8_t)(8u+row);
        fill(22,y,55,1,' ',ATTR_NORMAL);
        if(line<line_count){
            uint16_t number=(uint16_t)(line+1u);
            uint16_t start=asteros_editor_line_start(&editor_state,line);
            uint16_t length=asteros_editor_line_length(&editor_state,line);
            uint16_t column;
            cell(22,y,(char)('0'+(number/1000u)%10u),ATTR_PANEL);
            cell(23,y,(char)('0'+(number/100u)%10u),ATTR_PANEL);
            cell(24,y,(char)('0'+(number/10u)%10u),ATTR_PANEL);
            cell(25,y,(char)('0'+number%10u),ATTR_PANEL);
            cell(26,y,':',ATTR_STATUS);
            for(column=0;column<EDITOR_TEXT_WIDTH;column++){
                uint16_t source_column=(uint16_t)(scroll_column+column);
                if(source_column<length)
                    cell((uint8_t)(EDITOR_TEXT_X+column),y,
                         source[start+source_column],ATTR_NORMAL);
            }
            if(line==cursor_line&&cursor_column>=scroll_column&&
               cursor_column<scroll_column+EDITOR_TEXT_WIDTH){
                uint16_t visible=(uint16_t)(cursor_column-scroll_column);
                char value=cursor_column<length?source[start+cursor_column]:' ';
                cell((uint8_t)(EDITOR_TEXT_X+visible),y,value,ATTR_SELECT);
            }
        }
    }
    if(editor_status[0])
        text_n(22,18,editor_status,ATTR_STATUS,TERM_INPUT_MAX);
    if(editor_prompt_mode!=EDITOR_PROMPT_NONE){
        text(22,19,editor_prompt_mode==EDITOR_PROMPT_SAVE_AS?"Save as: ":"Open: ",ATTR_STATUS);
        text_n(31,19,editor_prompt_name,ATTR_NORMAL,ASTEROS_FS_NAME_MAX);
    }else{
        text(22,19,"F5 Check syntax | Ctrl+S save | Ctrl+O open",ATTR_STATUS);
    }
}

static void draw_files(void){
    uint8_t count=asteros_fs_count();
    uint8_t index;
    text(22,5,"RAM filesystem",ATTR_STATUS);
    if(asteros_volume_present)
        text(22,6,"8.3 files volatile | AsterOS volume found (read-only)",ATTR_NORMAL);
    else if(ata_disk_present)
        text(22,6,"8.3 files volatile | disk detected; no AsterOS volume",ATTR_NORMAL);
    else
        text(22,6,"8.3 files volatile | no ATA disk detected",ATTR_NORMAL);
    text(22,7,"NAME                         TYPE",ATTR_PANEL);
    if(count==0){
        text(22,9,"No files. Use IDE Ctrl+Alt+S to create one.",ATTR_NORMAL);
        files_selected=0;
    }else{
        if(files_selected>=count)files_selected=(uint8_t)(count-1u);
        for(index=0;index<count&&index<ASTEROS_FS_MAX_FILES;index++){
            asteros_fs_info_t info;
            uint8_t y=(uint8_t)(8u+index);
            uint8_t attribute=index==files_selected?ATTR_SELECT:ATTR_NORMAL;
            if(asteros_fs_stat(index,&info)!=ASTEROS_FS_OK)continue;
            fill(22,y,55,1,' ',attribute);
            text_n(23,y,info.name,attribute,ASTEROS_FS_NAME_MAX);
            text(42,y,"file",attribute);
        }
    }
    if(files_status[0])text_n(22,18,files_status,ATTR_STATUS,TERM_INPUT_MAX);
    text(22,19,"Up/Down select | Enter open in IDE | Esc close",ATTR_NORMAL);
}
static void draw_programs(void){
    uint8_t index;
    text(22,5,"Programs",ATTR_STATUS);
    text(22,7,"BUILT-IN APPLICATIONS",ATTR_STATUS);
    for(index=0;index<asteros_app_catalog_count();index++){
        const asteros_app_info_t *info=asteros_app_catalog_at(index);
        uint8_t y=(uint8_t)(8u+index);
        if(!info)continue;
        text_n(22,y,info->name,ATTR_NORMAL,11);
        text_n(34,y,info->summary,ATTR_NORMAL,44);
    }
    text(22,16,"Terminal: type help <app> for details and limitations.",ATTR_STATUS);
    text(22,18,"No arbitrary transient programs can be launched.",ATTR_NORMAL);
    text(22,19,"Q, X, Esc or Alt+F4 closes this window.",ATTR_NORMAL);
}
static void draw_about(void){text(22,5,"About AsterOS",ATTR_STATUS);text(22,7,"32-bit x86 experimental operating system",ATTR_NORMAL);text(22,8,"GUI shell over the CCP/BDOS direction",ATTR_NORMAL);text(22,10,"Enter opens the selected app.",ATTR_NORMAL);text(22,11,"Arrow keys or W/S navigate.",ATTR_NORMAL);text(22,12,"1-6 or T/F/P/A/C/I launches apps directly.",ATTR_NORMAL);text(22,13,"Q, X, Esc or Alt+F4 closes this window.",ATTR_NORMAL);}
static void draw_cursor_settings(void){
    text(22,5,"Accessibility and cursor settings",ATTR_STATUS);
    text(22,7,"Pointer visible [V]:",ATTR_NORMAL);
    text(43,7,cursor_visible?"On":"Off",cursor_visible?ATTR_STATUS:ATTR_PANEL);
    text(22,9,"Pointer speed [S]:",ATTR_NORMAL);
    if(cursor_speed==1)text(42,9,"1x",ATTR_STATUS);
    else if(cursor_speed==2)text(42,9,"2x",ATTR_STATUS);
    else text(42,9,"3x",ATTR_STATUS);
    text(22,11,"Cursor shape [C]:",ATTR_NORMAL);
    cell(40,11,cursor_glyphs[cursor_shape],cursor_attributes[cursor_color]);
    text(42,11,cursor_shape_names[cursor_shape],ATTR_STATUS);
    text(22,13,"Cursor color [K]:",ATTR_NORMAL);
    text(40,13,cursor_color_names[cursor_color],ATTR_STATUS);
    text(22,15,"Keyboard hover click [B]:",ATTR_NORMAL);
    text(47,15,keyboard_click_enabled?"On":"Off",keyboard_click_enabled?ATTR_STATUS:ATTR_PANEL);
    text(22,17,"Shift activates the hovered item.",ATTR_NORMAL);
    text(22,18,"Ctrl+Shift is reserved for right-click.",ATTR_NORMAL);
}
static void draw_desktop_surface(void){
    fill(19,2,61,21,' ',ATTR_DESKTOP);
    text(23,5,"ASTEROS",ATTR_TITLE);
    text(23,7,"This computer",ATTR_ICON);
    text(23,8,"[PC]",ATTR_ICON);
    text(33,7,"Documents",ATTR_ICON);
    text(33,8,"[DIR]",ATTR_ICON);
    text(43,7,"Programs",ATTR_ICON);
    text(43,8,"[APP]",ATTR_ICON);
    text(53,7,"Recycle",ATTR_ICON);
    text(53,8,"[BIN]",ATTR_ICON);
    text(63,7,"IDE",ATTR_ICON);
    text(63,8,"[SRC]",ATTR_ICON);
    text(23,17,"AsterOS desktop",ATTR_TITLE);
    text(23,18,"VGA text-mode workspace",ATTR_NORMAL);
}
static void draw_context_menu(void){
    uint8_t row;
    uint8_t attribute;
    if(!context_menu_open)return;
    fill(context_menu_x,context_menu_y,ASTEROS_CONTEXT_MENU_WIDTH,ASTEROS_CONTEXT_MENU_HEIGHT,' ',ATTR_PANEL);
    border(context_menu_x,context_menu_y,ASTEROS_CONTEXT_MENU_WIDTH,ASTEROS_CONTEXT_MENU_HEIGHT);
    for(row=ASTEROS_CONTEXT_MENU_OPEN;row<=ASTEROS_CONTEXT_MENU_CLOSE;row++){
        attribute=row==context_menu_selection?ATTR_SELECT:ATTR_PANEL;
        fill((uint8_t)(context_menu_x+1),(uint8_t)(context_menu_y+row),
             (uint8_t)(ASTEROS_CONTEXT_MENU_WIDTH-2),1,' ',attribute);
        if(row==ASTEROS_CONTEXT_MENU_OPEN)
            text((uint8_t)(context_menu_x+2),(uint8_t)(context_menu_y+row),"Open",attribute);
        else if(row==ASTEROS_CONTEXT_MENU_SETTINGS)
            text((uint8_t)(context_menu_x+2),(uint8_t)(context_menu_y+row),"Settings",attribute);
        else
            text((uint8_t)(context_menu_x+2),(uint8_t)(context_menu_y+row),
                 asteros_context_menu_close_targets_active_window(active_app,context_menu_target)?"Close window":"Show desktop",attribute);
    }
}
static void activate_app(uint8_t app){
    if(app>=ASTEROS_APP_COUNT)return;
    /* Terminal buffers are persistent app state, not scratch state for the
       currently focused window. Only explicitly closing Terminal clears it. */
    start_menu_open=0;
    run_dialog_open=0;
    run_length=0;
    run_buffer[0]='\0';
    selected=app;
    active_app=(int8_t)app;
}
static void start_menu_execute(uint8_t item){
    if(item<ASTEROS_START_MENU_ITEM_RUN){
        activate_app(start_menu_apps[item]);
    }else if(item==ASTEROS_START_MENU_ITEM_RUN){
        start_menu_open=0;
        run_dialog_open=1;
        run_length=0;
        run_buffer[0]='\0';
        set_short_status(run_status,"Enter a built-in application name");
    }
}
static void context_menu_execute(int8_t choice){
    if(choice==ASTEROS_CONTEXT_MENU_OPEN){
        if(context_menu_target>=0&&context_menu_target<ASTEROS_APP_COUNT)
            activate_app((uint8_t)context_menu_target);
    }else if(choice==ASTEROS_CONTEXT_MENU_SETTINGS){
        activate_app(ASTEROS_APP_SETTINGS);
    }else if(choice==ASTEROS_CONTEXT_MENU_CLOSE){
        if(asteros_context_menu_close_targets_active_window(active_app,
                                                            context_menu_target)){
            close_active_app();
        }else{
            /* A desktop or inactive-taskbar target returns to the desktop;
               it must not silently close a different foreground application. */
            active_app=-1;
        }
    }
    context_menu_open=0;
}
static void draw_taskbar(void){
    fill(0,23,WIDTH,2, ' ', ATTR_TASK);
    text(1,23,"[ START ]",ATTR_TASK_ACTIVE);
    text(12,23,"Terminal",active_app==0?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(22,23,"Files",active_app==1?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(29,23,"Programs",active_app==2?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(40,23,"About",active_app==3?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(47,23,"Settings",active_app==ASTEROS_APP_SETTINGS?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(56,23,"IDE",active_app==ASTEROS_APP_IDE?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(70,23,"AsterOS",ATTR_TASK_ACTIVE);
    text(2,24,active_app<0?"Desktop ready":"Window active",ATTR_TASK);
    text(67,24,"i386 PM",ATTR_TASK);
}
static void draw_start_menu(void){
    uint8_t row;
    if(!start_menu_open)return;
    fill(ASTEROS_START_MENU_X,ASTEROS_START_MENU_Y,
         ASTEROS_START_MENU_WIDTH,ASTEROS_START_MENU_HEIGHT,' ',ATTR_PANEL);
    border(ASTEROS_START_MENU_X,ASTEROS_START_MENU_Y,
           ASTEROS_START_MENU_WIDTH,ASTEROS_START_MENU_HEIGHT);
    text((uint8_t)(ASTEROS_START_MENU_X+2u),
         (uint8_t)(ASTEROS_START_MENU_Y+1u),"AsterOS Start",ATTR_TITLE);
    for(row=0;row<ASTEROS_START_MENU_ITEM_COUNT;row++){
        uint8_t y=(uint8_t)(ASTEROS_START_MENU_Y+2u+row);
        uint8_t attribute=row==start_menu_selection?ATTR_SELECT:ATTR_PANEL;
        fill((uint8_t)(ASTEROS_START_MENU_X+1u),y,
             (uint8_t)(ASTEROS_START_MENU_WIDTH-2u),1,' ',attribute);
        text((uint8_t)(ASTEROS_START_MENU_X+3u),y,start_menu_items[row],attribute);
    }
}
static void draw_run_dialog(void){
    if(!run_dialog_open)return;
    fill(20,8,42,8,' ',ATTR_PANEL);
    border(20,8,42,8);
    text(22,9,"Run AsterOS application",ATTR_TITLE);
    text(22,11,"Application:",ATTR_NORMAL);
    text_n(35,11,run_buffer,ATTR_STATUS,25);
    text(22,13,"Built-in apps only; no program loader.",ATTR_NORMAL);
    if(run_status[0])text_n(22,14,run_status,ATTR_STATUS,38);
    else text(22,14,"Examples: terminal, files, IDE, settings.",ATTR_NORMAL);
}
static void draw_selected_app(void){
    fill(21,4,57,18,' ',ATTR_NORMAL);
    if(active_app<0){
        text(22,5,"Select an application",ATTR_STATUS);
        text(22,7,"Use W/S or arrow keys and press Enter.",ATTR_NORMAL);
        text(22,8,"Press 1-6 or T/F/P/A/C/I to launch an app.",ATTR_NORMAL);
        text(22,10,"Desktop applications",ATTR_STATUS);
        text(22,12,"[T] Terminal   [F] Files",ATTR_NORMAL);
        text(22,13,"[P] Programs  [A] About",ATTR_NORMAL);
        text(22,14,"[C] Cursor Settings  [I] IDE",ATTR_NORMAL);
        text(22,16,"Ctrl+Esc or click Start opens the Start menu.",ATTR_NORMAL);
        text(22,17,"Run opens built-in apps by name only.",ATTR_NORMAL);
    }else{
        fill(21,4,57,2,' ',ATTR_TITLE);
        text(23,4,items[(uint8_t)active_app],ATTR_TITLE);
        text(73,4,"[X]",ATTR_TITLE);
        if(active_app==0)draw_terminal();
        else if(active_app==1)draw_files();
        else if(active_app==2)draw_programs();
        else if(active_app==3)draw_about();
        else if(active_app==ASTEROS_APP_SETTINGS)draw_cursor_settings();
        else draw_editor();
    }
}
void gui_draw(void){cursor_clear();fill(0,0,WIDTH,HEIGHT,' ',ATTR_NORMAL);fill(0,0,WIDTH,1,' ',ATTR_TITLE);text(2,0,"AsterOS",ATTR_TITLE);text(68,0,"Desktop",ATTR_TITLE);border(1,2,18,19);text(3,3,"Applications",ATTR_PANEL);for(uint8_t i=0;i<ASTEROS_APP_COUNT;i++)text(3,(uint8_t)(5+i*2),items[i],i==selected?ATTR_SELECT:ATTR_PANEL);draw_desktop_surface();border(20,2,59,19);text(22,3,"Welcome to AsterOS",ATTR_NORMAL);draw_selected_app();draw_taskbar();draw_start_menu();draw_run_dialog();draw_context_menu();cursor_draw();}
void gui_init(void){ata_disk_present=0;asteros_volume_present=0;selected=0;keyboard_click_enabled=1;start_menu_open=0;start_menu_selection=0;run_dialog_open=0;run_length=0;run_buffer[0]='\0';run_status[0]='\0';context_menu_open=0;context_menu_selection=ASTEROS_CONTEXT_MENU_OPEN;context_menu_target=ASTEROS_HIT_NONE;active_app=-1;mouse_x=WIDTH*2;mouse_y=HEIGHT*2;cursor_active=0;cursor_visible=1;cursor_speed=1;cursor_shape=0;cursor_color=0;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_history_clear(&terminal_history_state);terminal_history_output[0]='\0';terminal_reset_input();editor_prompt_mode=EDITOR_PROMPT_NONE;editor_prompt_length=0;editor_status[0]='\0';asteros_ide_file_init(&editor_file,"HELLO.TL");set_short_status(files_status,"RAM files are volatile");files_selected=0;(void)asteros_fs_write(editor_file.name,(const uint8_t *)editor_default_source,(uint16_t)(sizeof editor_default_source-1u));asteros_editor_init(&editor_state,editor_default_source);editor_scroll_line=0;gui_draw();}
void gui_set_ata_disk_present(uint8_t present){ata_disk_present=(uint8_t)(present!=0);}
void gui_set_asteros_volume_present(uint8_t present){asteros_volume_present=(uint8_t)(present!=0);}
void gui_handle_key(char key){
    uint8_t event=(uint8_t)key;
    if(event==ASTEROS_KEY_START_MENU){
        context_menu_open=0;
        if(run_dialog_open){
            run_dialog_open=0;
            run_length=0;
            run_buffer[0]='\0';
            run_status[0]='\0';
        }else{
            start_menu_open=(uint8_t)!start_menu_open;
            if(start_menu_open)start_menu_selection=0;
        }
        gui_draw();
        return;
    }
    if(event==ASTEROS_KEY_CLOSE_APP){
        context_menu_open=0;
        if(run_dialog_open){
            run_dialog_open=0;
            run_length=0;
            run_buffer[0]='\0';
        }else if(start_menu_open){
            start_menu_open=0;
        }else if(active_app>=0){
            close_active_app();
        }
        gui_draw();
        return;
    }
    if(event==ASTEROS_KEY_HOVER_LEFT_CLICK){
        if(run_dialog_open)return;
        if(active_app==ASTEROS_APP_IDE&&editor_prompt_mode!=EDITOR_PROMPT_NONE)return;
        if(keyboard_click_enabled){gui_handle_mouse(0,0,0);gui_handle_mouse(0,0,1);gui_handle_mouse(0,0,0);}
        return;
    }
    if(event==ASTEROS_KEY_HOVER_RIGHT_CLICK){
        if(run_dialog_open)return;
        if(active_app==ASTEROS_APP_IDE&&editor_prompt_mode!=EDITOR_PROMPT_NONE)return;
        start_menu_open=0;
        context_menu_open=1;
        context_menu_selection=ASTEROS_CONTEXT_MENU_OPEN;
        context_menu_x=(uint8_t)(mouse_x/4);
        context_menu_y=(uint8_t)(mouse_y/4);
        context_menu_target=asteros_context_menu_resolve_target(
            active_app,context_menu_x,context_menu_y,
            asteros_desktop_hit_test(active_app,context_menu_x,context_menu_y));
        asteros_context_menu_place(context_menu_x,context_menu_y,
                                   &context_menu_x,&context_menu_y);
        gui_draw();
        return;
    }
    if(context_menu_open){
        if(key==27||key=='q'||key=='Q'){
            context_menu_open=0;
            gui_draw();
            return;
        }
        if(event==KEY_UP||key=='w'||key=='W'){
            context_menu_selection=asteros_context_menu_move_selection(context_menu_selection,-1);
            gui_draw();
            return;
        }
        if(event==KEY_DOWN||key=='s'||key=='S'){
            context_menu_selection=asteros_context_menu_move_selection(context_menu_selection,1);
            gui_draw();
            return;
        }
        if(key=='\r'||key=='\n'||key==' '){
            context_menu_execute(context_menu_selection);
            gui_draw();
            return;
        }
        return;
    }
    if(run_dialog_open){
        if(key==27){
            run_dialog_open=0;
            run_length=0;
            run_buffer[0]='\0';
            run_status[0]='\0';
        }else if(key=='\b'){
            if(run_length){
                run_length--;
                run_buffer[run_length]='\0';
            }
            run_status[0]='\0';
        }else if(key=='\r'||key=='\n'){
            int8_t app=asteros_app_resolve_command(run_buffer);
            if(app>=ASTEROS_APP_TERMINAL&&app<ASTEROS_APP_COUNT){
                activate_app((uint8_t)app);
            }else{
                set_short_status(run_status,"Unknown app; try terminal, files, IDE or settings");
            }
        }else if(key>=32&&key<=126){
            if(run_length<TERM_INPUT_MAX){
                run_buffer[run_length++]=key;
                run_buffer[run_length]='\0';
                run_status[0]='\0';
            }
        }else return;
        gui_draw();
        return;
    }
    if(start_menu_open){
        if(key==27){
            start_menu_open=0;
        }else if(event==KEY_UP||key=='w'||key=='W'){
            start_menu_selection=asteros_start_menu_move_selection(
                start_menu_selection,-1);
        }else if(event==KEY_DOWN||key=='s'||key=='S'){
            start_menu_selection=asteros_start_menu_move_selection(
                start_menu_selection,1);
        }else if(key=='\r'||key=='\n'||key==' '){
            start_menu_execute(start_menu_selection);
        }else return;
        gui_draw();
        return;
    }
    if(active_app==ASTEROS_APP_SETTINGS&&(key=='b'||key=='B')){
        keyboard_click_enabled=(uint8_t)!keyboard_click_enabled;
        gui_draw();
        return;
    }
    if(active_app<0){
        if(event==KEY_UP||key=='w'||key=='W'){
            if(selected==0)selected=(uint8_t)(ASTEROS_APP_COUNT-1u);else--selected;
            gui_draw();
        }else if(event==KEY_DOWN||key=='s'||key=='S'){
            selected=(uint8_t)((selected+1u)%ASTEROS_APP_COUNT);
            gui_draw();
        }else if(key>='1'&&key<='6'){
            selected=(uint8_t)(key-'1');activate_app(selected);gui_draw();
        }else if(key=='t'||key=='T'||key=='f'||key=='F'||key=='p'||key=='P'||key=='a'||key=='A'||key=='c'||key=='C'||key=='i'||key=='I'){
            if(key=='t'||key=='T')selected=ASTEROS_APP_TERMINAL;
            else if(key=='f'||key=='F')selected=ASTEROS_APP_FILES;
            else if(key=='p'||key=='P')selected=ASTEROS_APP_PROGRAMS;
            else if(key=='a'||key=='A')selected=ASTEROS_APP_ABOUT;
            else if(key=='c'||key=='C')selected=ASTEROS_APP_SETTINGS;
            else selected=ASTEROS_APP_IDE;
            activate_app(selected);gui_draw();
        }else if(key=='\r'||key=='\n'){
            activate_app(selected);gui_draw();
        }
        return;
    }
    if(key==KEY_TAB){
        selected=(uint8_t)((selected+1u)%ASTEROS_APP_COUNT);
        activate_app(selected);gui_draw();return;
    }
    if(active_app==ASTEROS_APP_IDE&&editor_prompt_mode!=EDITOR_PROMPT_NONE&&key==27){
        editor_prompt_mode=EDITOR_PROMPT_NONE;
        set_short_status(editor_status,"Filename operation cancelled");
        gui_draw();
        return;
    }
    if(key==27||(active_app!=ASTEROS_APP_IDE&&(key=='q'||key=='Q'||key=='x'||key=='X'))){
        close_active_app();gui_draw();return;
    }
    if(active_app==ASTEROS_APP_SETTINGS){
        if(key=='v'||key=='V')cursor_setting_change(0);
        else if(key=='s'||key=='S')cursor_setting_change(1);
        else if(key=='c'||key=='C')cursor_setting_change(2);
        else if(key=='k'||key=='K')cursor_setting_change(3);
        else return;
        gui_draw();return;
    }
    if(active_app==ASTEROS_APP_FILES){
        uint8_t count=asteros_fs_count();
        if(event==KEY_UP){
            if(files_selected>0)files_selected--;
            else if(count)files_selected=(uint8_t)(count-1u);
        }else if(event==KEY_DOWN){
            if(count&&files_selected+1u<count)files_selected++;
            else if(count)files_selected=0;
        }else if(key=='\r'||key=='\n'){
            files_open_selected();
        }else return;
        gui_draw();return;
    }
    if(active_app==ASTEROS_APP_IDE){
        if(event==ASTEROS_KEY_SAVE){
            (void)editor_save_named_file(editor_file.name);
            gui_draw();return;
        }
        if(event==ASTEROS_KEY_SAVE_AS){
            editor_begin_prompt(EDITOR_PROMPT_SAVE_AS);
            gui_draw();return;
        }
        if(event==ASTEROS_KEY_OPEN){
            editor_begin_prompt(EDITOR_PROMPT_OPEN);
            gui_draw();return;
        }
        if(editor_prompt_mode!=EDITOR_PROMPT_NONE){
            if(key=='\b'){
                if(editor_prompt_length){
                    editor_prompt_length--;
                    editor_prompt_name[editor_prompt_length]='\0';
                }
            }else if(key=='\r'||key=='\n'){
                editor_finish_prompt();
            }else if(key>=32&&key<=126){
                if(editor_filename_character(key)&&
                   editor_prompt_length<ASTEROS_FS_NAME_MAX){
                    editor_prompt_name[editor_prompt_length++]=key;
                    editor_prompt_name[editor_prompt_length]='\0';
                }else{
                    set_short_status(editor_status,"Use an 8.3 filename (letters, digits, _ or -)");
                }
            }else return;
            gui_draw();return;
        }
        if(event==ASTEROS_KEY_CHECK){
            editor_check_source();
            gui_draw();return;
        }
        if(event==ASTEROS_KEY_LEFT)asteros_editor_move_left(&editor_state);
        else if(event==ASTEROS_KEY_RIGHT)asteros_editor_move_right(&editor_state);
        else if(event==ASTEROS_KEY_UP)asteros_editor_move_up(&editor_state);
        else if(event==ASTEROS_KEY_DOWN)asteros_editor_move_down(&editor_state);
        else if(event==ASTEROS_KEY_HOME)asteros_editor_move_home(&editor_state);
        else if(event==ASTEROS_KEY_END)asteros_editor_move_end(&editor_state);
        else if(event==ASTEROS_KEY_DELETE)asteros_editor_delete(&editor_state);
        else if(key=='\b')asteros_editor_backspace(&editor_state);
        else if(key=='\r'||key=='\n')asteros_editor_insert(&editor_state,'\n');
        else if(key>=32&&key<=126)asteros_editor_insert(&editor_state,key);
        else return;
        editor_status[0]='\0';
        gui_draw();return;
    }
    if(active_app!=ASTEROS_APP_TERMINAL)return;
    if(key=='\b'){
        if(terminal_length){terminal_length--;terminal_buffer[terminal_length]='\0';gui_draw();}
    }else if(key=='\r'||key=='\n'){
        terminal_execute();gui_draw();
    }else if(key>=32&&key<=126){
        if(terminal_length<TERM_INPUT_MAX){terminal_buffer[terminal_length++]=key;terminal_buffer[terminal_length]='\0';gui_draw();}
    }
}


void gui_handle_mouse(int8_t dx,int8_t dy,uint8_t buttons){
    uint8_t cell_x;
    uint8_t cell_y;
    int8_t target;
    int8_t menu_choice;
    uint8_t redraw=0;
    uint8_t menu_handled=0;
    uint8_t pressed=(uint8_t)(buttons&0x07);
    uint8_t context_was_open=context_menu_open;
    static uint8_t previous_buttons;
    uint8_t right_edge;
    cursor_clear();
    mouse_x+=(int16_t)dx*(int16_t)cursor_speed;
    mouse_y-=(int16_t)dy*(int16_t)cursor_speed;
    if(mouse_x<0)mouse_x=0;
    if(mouse_y<0)mouse_y=0;
    if(mouse_x>(WIDTH*4)-1)mouse_x=(WIDTH*4)-1;
    if(mouse_y>(HEIGHT*4)-1)mouse_y=(HEIGHT*4)-1;
    cell_x=(uint8_t)(mouse_x/4);
    cell_y=(uint8_t)(mouse_y/4);
    target=asteros_desktop_hit_test(active_app,cell_x,cell_y);
    right_edge=(uint8_t)((pressed&2u)&&!(previous_buttons&2u));
    if(run_dialog_open){
        previous_buttons=pressed;
        cursor_draw();
        return;
    }
    if(right_edge&&start_menu_open)start_menu_open=0;
    if(right_edge){
        context_menu_open=1;
        context_menu_selection=ASTEROS_CONTEXT_MENU_OPEN;
        asteros_context_menu_place(cell_x,cell_y,
                                   &context_menu_x,&context_menu_y);
        context_menu_target=asteros_context_menu_resolve_target(active_app,cell_x,cell_y,target);
        redraw=1;
    }else if(context_menu_open&&asteros_mouse_activation_edge(previous_buttons,pressed)){
        int8_t choice=asteros_context_menu_hit_test(context_menu_x,context_menu_y,
                                                    cell_x,cell_y);
        if(choice!=ASTEROS_CONTEXT_MENU_NONE)
            context_menu_execute(choice);
        else
            context_menu_open=0;
        /* Clicking the border or elsewhere only dismisses the menu. */
        redraw=1;
    }
    if(!right_edge&&!context_was_open&&!context_menu_open&&start_menu_open){
        menu_choice=asteros_start_menu_hit_test(cell_x,cell_y);
        if(menu_choice>=0){
            if(start_menu_selection!=(uint8_t)menu_choice){
                start_menu_selection=(uint8_t)menu_choice;
                redraw=1;
            }
            if(asteros_mouse_activation_edge(previous_buttons,pressed)){
                start_menu_execute((uint8_t)menu_choice);
                redraw=1;
                menu_handled=1;
            }
        }else if(asteros_mouse_activation_edge(previous_buttons,pressed)){
            start_menu_open=0;
            redraw=1;
            if(target==ASTEROS_HIT_START)menu_handled=1;
        }
    }
    if(!start_menu_open&&active_app<0&&target>=0&&
       target<ASTEROS_APP_COUNT&&selected!=(uint8_t)target){
        selected=(uint8_t)target;
        redraw=1;
    }
    if(!right_edge&&!context_was_open&&!context_menu_open&&!menu_handled&&
       asteros_mouse_activation_edge(previous_buttons,pressed)){
        if(target==ASTEROS_HIT_START){
            start_menu_open=1;
            start_menu_selection=0;
            redraw=1;
        }else if(target==ASTEROS_HIT_CLOSE){
            close_active_app();
            redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_VISIBLE){cursor_setting_change(0);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_SPEED){cursor_setting_change(1);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_SHAPE){cursor_setting_change(2);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_COLOR){cursor_setting_change(3);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_KEYBOARD_CLICK){keyboard_click_enabled=(uint8_t)!keyboard_click_enabled;redraw=1;
        }else if(target>=0&&target<ASTEROS_APP_COUNT){
            if(asteros_taskbar_should_minimize(active_app,target,cell_y)){
                active_app=-1;
            }else{
                activate_app((uint8_t)target);
            }
            redraw=1;
        }
    }
    previous_buttons=pressed;
    if(redraw)gui_draw();
    else cursor_draw();
}
