#include "gui.h"
#include "terminal.h"
#include "desktop_input.h"
#include "keyboard.h"
#define VGA ((volatile uint16_t*)0xB8000)
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
#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_TAB 0x09

static uint8_t selected;
static uint8_t terminal_result;
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
static const char *items[] = {"Terminal", "Files", "Programs", "About", "Settings"};

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
static void terminal_execute(void){
    char *command=terminal_buffer;
    const char *arguments;
    asteros_command_t parsed;
    uint8_t repeat_requested=0;
    uint8_t repeat_succeeded=0;

    terminal_output[0]='\0';
    terminal_history_output[0]='\0';
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
        case ASTEROS_COMMAND_HELP: terminal_result=TERM_HELP; break;
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
            text(22,9,"help clear/cls apps/ls info/about ver uname pwd",ATTR_NORMAL);
            text(22,10,"echo history/history clear menu/exit",ATTR_NORMAL);
            text(22,11,"Commands are case-insensitive; extra args rejected.",ATTR_NORMAL);
            text(22,12,"echo accepts repeated spaces before text.",ATTR_NORMAL);
            text(22,13,"!! repeats the previous command.",ATTR_NORMAL);
            text(22,14,"history shows the previous command.",ATTR_NORMAL);
            text(22,15,"history clear forgets the previous command.",ATTR_NORMAL);
            text(22,16,"pwd returns a placeholder path (no filesystem yet).",ATTR_NORMAL);
            text(22,17,"apps/ls lists built-in apps, not disk files.",ATTR_NORMAL);
            text(22,18,"uname reports i386; menu/exit returns to launcher.",ATTR_NORMAL);
            break;
        case TERM_APPS:
            text(22,9,"Terminal Files Programs About Settings",ATTR_NORMAL);
            break;
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
    text(22,19,"Demo shell; no real filesystem/process services.",ATTR_STATUS);
}
static void draw_files(void){text(22,5,"Files",ATTR_STATUS);text(22,7,"NAME                 TYPE",ATTR_NORMAL);text(22,8,"README.MD             text",ATTR_NORMAL);text(22,9,"KERNEL.BIN            system",ATTR_NORMAL);text(22,10,"PROGRAMS/             directory",ATTR_NORMAL);text(22,12,"Presentation only; no filesystem service",ATTR_NORMAL);text(22,13,"Press Q, X or Esc to close this window.",ATTR_NORMAL);}
static void draw_programs(void){text(22,5,"Programs",ATTR_STATUS);text(22,7,"BUILT-IN GUI APPLICATIONS",ATTR_NORMAL);text(22,8,"Terminal   interactive command view",ATTR_NORMAL);text(22,9,"Files      presentation-only listing",ATTR_NORMAL);text(22,10,"Programs   application inventory",ATTR_NORMAL);text(22,11,"About      OS/runtime information",ATTR_NORMAL);text(22,13,"Press Q, X or Esc to close this window.",ATTR_NORMAL);text(22,14,"No transient process loader yet.",ATTR_NORMAL);}
static void draw_about(void){text(22,5,"About AsterOS",ATTR_STATUS);text(22,7,"32-bit x86 experimental operating system",ATTR_NORMAL);text(22,8,"GUI shell over the CCP/BDOS direction",ATTR_NORMAL);text(22,10,"Enter opens the selected app.",ATTR_NORMAL);text(22,11,"Arrow keys or W/S navigate.",ATTR_NORMAL);text(22,12,"1-5 or T/F/P/A/C launches apps directly.",ATTR_NORMAL);text(22,13,"Q, X or Esc closes the active window.",ATTR_NORMAL);}
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
    text(23,17,"AsterOS desktop",ATTR_TITLE);
    text(23,18,"VGA text-mode workspace",ATTR_NORMAL);
}
static void draw_context_menu(void){
    if(!context_menu_open)return;
    fill(context_menu_x,context_menu_y,16,5,' ',ATTR_PANEL);
    border(context_menu_x,context_menu_y,16,5);
    text((uint8_t)(context_menu_x+2),(uint8_t)(context_menu_y+1),"Open",ATTR_PANEL);
    text((uint8_t)(context_menu_x+2),(uint8_t)(context_menu_y+2),"Settings",ATTR_PANEL);
    text((uint8_t)(context_menu_x+2),(uint8_t)(context_menu_y+3),active_app>=0?"Close window":"Show desktop",ATTR_PANEL);
}
static void draw_taskbar(void){
    fill(0,23,WIDTH,2, ' ', ATTR_TASK);
    text(1,23,"[ START ]",ATTR_TASK_ACTIVE);
    text(12,23,"Terminal",active_app==0?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(22,23,"Files",active_app==1?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(29,23,"Programs",active_app==2?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(40,23,"About",active_app==3?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(47,23,"Settings",active_app==4?ATTR_TASK_ACTIVE:ATTR_TASK);
    text(70,23,"AsterOS",ATTR_TASK_ACTIVE);
    text(2,24,active_app<0?"Desktop ready":"Window active",ATTR_TASK);
    text(67,24,"i386 PM",ATTR_TASK);
}
static void draw_selected_app(void){
    fill(21,4,57,18,' ',ATTR_NORMAL);
    if(active_app<0){
        text(22,5,"Select an application",ATTR_STATUS);
        text(22,7,"Use W/S or arrow keys and press Enter.",ATTR_NORMAL);
        text(22,8,"Press 1-4 or T/F/P/A to launch an app.",ATTR_NORMAL);
        text(22,10,"Desktop applications",ATTR_STATUS);
        text(22,12,"[T] Terminal   [F] Files",ATTR_NORMAL);
        text(22,13,"[P] Programs  [A] About",ATTR_NORMAL);
        text(22,14,"[C] Cursor Settings",ATTR_NORMAL);
    }else{
        fill(21,4,57,2,' ',ATTR_TITLE);
        text(23,4,items[(uint8_t)active_app],ATTR_TITLE);
        text(73,4,"[X]",ATTR_TITLE);
        if(active_app==0)draw_terminal();
        else if(active_app==1)draw_files();
        else if(active_app==2)draw_programs();
        else if(active_app==3)draw_about();
        else draw_cursor_settings();
    }
}
void gui_draw(void){cursor_clear();fill(0,0,WIDTH,HEIGHT,' ',ATTR_NORMAL);fill(0,0,WIDTH,1,' ',ATTR_TITLE);text(2,0,"AsterOS",ATTR_TITLE);text(68,0,"Desktop",ATTR_TITLE);border(1,2,18,19);text(3,3,"Applications",ATTR_PANEL);for(uint8_t i=0;i<5;i++)text(3,(uint8_t)(5+i*2),items[i],i==selected?ATTR_SELECT:ATTR_PANEL);draw_desktop_surface();border(20,2,59,19);text(22,3,"Welcome to AsterOS",ATTR_NORMAL);draw_selected_app();draw_taskbar();draw_context_menu();cursor_draw();}
void gui_init(void){selected=0;keyboard_click_enabled=1;context_menu_open=0;context_menu_target=ASTEROS_HIT_NONE;active_app=-1;mouse_x=WIDTH*2;mouse_y=HEIGHT*2;cursor_active=0;cursor_visible=1;cursor_speed=1;cursor_shape=0;cursor_color=0;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_history_clear(&terminal_history_state);terminal_history_output[0]='\0';terminal_reset_input();gui_draw();}
void gui_handle_key(char key){
    if((uint8_t)key==ASTEROS_KEY_HOVER_LEFT_CLICK){
        if(keyboard_click_enabled){gui_handle_mouse(0,0,0);gui_handle_mouse(0,0,1);gui_handle_mouse(0,0,0);}
        return;
    }
    if((uint8_t)key==ASTEROS_KEY_HOVER_RIGHT_CLICK){
        context_menu_open=1;
        context_menu_x=(uint8_t)(mouse_x/4);
        context_menu_y=(uint8_t)(mouse_y/4);
        if(context_menu_x>WIDTH-16)context_menu_x=WIDTH-16;
        if(context_menu_y>HEIGHT-5)context_menu_y=HEIGHT-5;
        context_menu_target=asteros_desktop_hit_test(active_app,context_menu_x,context_menu_y);
        gui_draw();
        return;
    }
    if(context_menu_open&&(key==27||key=='q'||key=='Q')){context_menu_open=0;gui_draw();return;}
    if(active_app==ASTEROS_APP_SETTINGS&&(key=='b'||key=='B')){keyboard_click_enabled=(uint8_t)!keyboard_click_enabled;gui_draw();return;}
    if(active_app<0){
        if((uint8_t)key==KEY_UP||key=='w'||key=='W'){if(selected==0)selected=4;else--selected;gui_draw();}
        else if((uint8_t)key==KEY_DOWN||key=='s'||key=='S'){selected=(uint8_t)((selected+1)%5);gui_draw();}
        else if(key>='1'&&key<='5'){selected=(uint8_t)(key-'1');active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();}
        else if(key=='t'||key=='T'||key=='f'||key=='F'||key=='p'||key=='P'||key=='a'||key=='A'||key=='c'||key=='C'){
            if(key=='t'||key=='T')selected=0;
            else if(key=='f'||key=='F')selected=1;
            else if(key=='p'||key=='P')selected=2;
            else if(key=='a'||key=='A')selected=3;
            else selected=4;
            active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();
        }
        else if(key=='\r'||key=='\n'){active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();}
        return;
    }
    if(key==KEY_TAB){selected=(uint8_t)((selected+1)%5);active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();return;}
    if(key=='q'||key=='Q'||key=='x'||key=='X'||key==27){active_app=-1;terminal_reset_input();gui_draw();return;}
    if(active_app==4){
        if(key=='v'||key=='V')cursor_setting_change(0);
        else if(key=='s'||key=='S')cursor_setting_change(1);
        else if(key=='c'||key=='C')cursor_setting_change(2);
        else if(key=='k'||key=='K')cursor_setting_change(3);
        else return;
        gui_draw();
        return;
    }
    if(active_app!=0)return;
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
    uint8_t redraw=0;
    uint8_t pressed=(uint8_t)(buttons&0x07);
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
    if(right_edge){
        context_menu_open=1;
        context_menu_x=cell_x;
        context_menu_y=cell_y;
        if(context_menu_x>WIDTH-16)context_menu_x=WIDTH-16;
        if(context_menu_y>HEIGHT-5)context_menu_y=HEIGHT-5;
        context_menu_target=target;
        redraw=1;
    }else if(context_menu_open&&asteros_mouse_activation_edge(previous_buttons,pressed)){
        if(cell_x>=context_menu_x&&cell_x<context_menu_x+16&&cell_y>context_menu_y&&cell_y<context_menu_y+4){
            uint8_t choice=(uint8_t)(cell_y-context_menu_y);
            if(choice==1){
                if(context_menu_target>=0&&context_menu_target<5){selected=(uint8_t)context_menu_target;active_app=context_menu_target;terminal_result=TERM_READY;terminal_output[0]='\\0';terminal_reset_input();}
            }else if(choice==2){selected=ASTEROS_APP_SETTINGS;active_app=ASTEROS_APP_SETTINGS;}
            else if(choice==3){active_app=-1;terminal_reset_input();}
            context_menu_open=0;
            redraw=1;
        }else{
            context_menu_open=0;
            /* A click outside the menu dismisses it without activating through it. */
            redraw=1;
        }
    }
    if(active_app<0&&target>=0&&target<5&&selected!=(uint8_t)target){
        selected=(uint8_t)target;
        redraw=1;
    }
    if(!right_edge&&!context_menu_open&&asteros_mouse_activation_edge(previous_buttons,pressed)){
        if(target==ASTEROS_HIT_START){
            active_app=-1;
            redraw=1;
        }else if(target==ASTEROS_HIT_CLOSE){
            active_app=-1;
            terminal_reset_input();
            redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_VISIBLE){cursor_setting_change(0);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_SPEED){cursor_setting_change(1);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_SHAPE){cursor_setting_change(2);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_COLOR){cursor_setting_change(3);redraw=1;
        }else if(target==ASTEROS_HIT_SETTING_KEYBOARD_CLICK){keyboard_click_enabled=(uint8_t)!keyboard_click_enabled;redraw=1;
        }else if(target>=0&&target<5){
            selected=(uint8_t)target;
            active_app=target;
            terminal_result=TERM_READY;
            terminal_output[0]='\0';
            terminal_reset_input();
            redraw=1;
        }
    }
    previous_buttons=pressed;
    if(redraw)gui_draw();
    else cursor_draw();
}
