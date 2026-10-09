#include "gui.h"
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
#define TERM_INPUT_MAX (TERM_W - 8)
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
static const char cursor_glyphs[] = {'*', '+', 'X', '>'};
static const char *cursor_shape_names[] = {"Star", "Plus", "Cross", "Arrow"};
static const uint8_t cursor_attributes[] = {0x0F, 0x0E, 0x0B, 0x0C, 0x0D};
static const char *cursor_color_names[] = {"White", "Yellow", "Cyan", "Red", "Magenta"};
static char terminal_buffer[TERM_INPUT_MAX + 1];
static char terminal_output[TERM_INPUT_MAX + 1];
static char terminal_last[TERM_INPUT_MAX + 1];
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
static uint8_t text_equals(const char *a,const char *b){while(*a&&*b){if(*a++!=*b++)return 0;}return (uint8_t)(*a==*b);}
static uint8_t starts_with(const char *text_value,const char *prefix){while(*prefix){if(*text_value++!=*prefix++)return 0;}return 1;}
static void copy_text(char *destination,const char *source){uint8_t i=0;while(source[i]&&i<TERM_INPUT_MAX){destination[i]=source[i];i++;}destination[i]='\0';}
static void trim_leading_spaces(char **value){while(**value==' ')(*value)++;}
static void terminal_reset_input(void){terminal_length=0;terminal_buffer[0]='\0';}
static void terminal_execute(void){
    char *command=terminal_buffer;
    terminal_output[0]='\0';
    trim_leading_spaces(&command);
    if(text_equals(command,"!!")&&terminal_last[0]){
        copy_text(terminal_buffer,terminal_last);
        command=terminal_buffer;
        trim_leading_spaces(&command);
    }
    if(text_equals(command,"history clear")){
        terminal_last[0]='\0';
        terminal_result=TERM_HISTORY;
        terminal_reset_input();
        return;
    }
    if(*command)copy_text(terminal_last,command);
    if(text_equals(command,"help")) terminal_result=TERM_HELP;
    else if(text_equals(command,"clear")||text_equals(command,"cls")) terminal_result=TERM_READY;
    else if(text_equals(command,"apps")||text_equals(command,"ls")) terminal_result=TERM_APPS;
    else if(text_equals(command,"info")||text_equals(command,"about")) terminal_result=TERM_INFO;
    else if(text_equals(command,"ver")) terminal_result=TERM_VER;
    else if(text_equals(command,"uname")) terminal_result=TERM_UNAME;
    else if(text_equals(command,"pwd")) terminal_result=TERM_PWD;
    else if(text_equals(command,"history")) terminal_result=TERM_HISTORY;
    else if(text_equals(command,"echo")) terminal_result=TERM_ECHO;
    else if(starts_with(command,"echo ")) {
        command += 5;
        trim_leading_spaces(&command);
        copy_text(terminal_output,command);
        terminal_result=TERM_ECHO;
    }
    else if(text_equals(command,"menu")||text_equals(command,"exit")) {
        active_app=-1;
        terminal_result=TERM_READY;
    }
    else if(*command) terminal_result=TERM_UNKNOWN;
    else terminal_result=TERM_READY;
    terminal_reset_input();
}
static void draw_terminal_result(void){
    switch(terminal_result){
        case TERM_HELP:
            text(22,9,"help clear/cls apps/ls info/about ver uname pwd",ATTR_NORMAL);
            text(22,10,"echo history/history clear menu/exit",ATTR_NORMAL);
            text(22,11,"Commands ignore leading spaces.",ATTR_NORMAL);
            text(22,12,"echo accepts repeated spaces before text.",ATTR_NORMAL);
            text(22,13,"!! repeats the previous command.",ATTR_NORMAL);
            text(22,14,"history shows the previous command.",ATTR_NORMAL);
            text(22,15,"history clear forgets the previous command.",ATTR_NORMAL);
            text(22,16,"pwd shows the current filesystem path.",ATTR_NORMAL);
            text(22,17,"uname shows the kernel architecture.",ATTR_NORMAL);
            text(22,18,"menu/exit returns to the application launcher.",ATTR_NORMAL);
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
            if(terminal_last[0]){text(22,9,"last: ",ATTR_STATUS);text_n(28,9,terminal_last,ATTR_NORMAL,TERM_INPUT_MAX-6);}else{text(22,9,"No previous command.",ATTR_NORMAL);}
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
}
static void draw_files(void){text(22,5,"Files",ATTR_STATUS);text(22,7,"NAME                 TYPE",ATTR_NORMAL);text(22,8,"README.MD             text",ATTR_NORMAL);text(22,9,"KERNEL.BIN            system",ATTR_NORMAL);text(22,10,"PROGRAMS/             directory",ATTR_NORMAL);text(22,12,"Read-only filesystem view",ATTR_NORMAL);text(22,13,"Press Q, X or Esc to close this window.",ATTR_NORMAL);}
static void draw_programs(void){text(22,5,"Programs",ATTR_STATUS);text(22,7,"BUILT-IN PROGRAMS",ATTR_NORMAL);text(22,8,"Terminal   interactive command view",ATTR_NORMAL);text(22,9,"Files      read-only filesystem view",ATTR_NORMAL);text(22,10,"Programs   application inventory",ATTR_NORMAL);text(22,11,"About      OS/runtime information",ATTR_NORMAL);text(22,13,"Press Q, X or Esc to close this window.",ATTR_NORMAL);}
static void draw_about(void){text(22,5,"About AsterOS",ATTR_STATUS);text(22,7,"32-bit x86 experimental operating system",ATTR_NORMAL);text(22,8,"GUI shell over the CCP/BDOS direction",ATTR_NORMAL);text(22,10,"Enter opens the selected app.",ATTR_NORMAL);text(22,11,"Arrow keys or W/S navigate.",ATTR_NORMAL);text(22,12,"1-5 or T/F/P/A/C launches apps directly.",ATTR_NORMAL);text(22,13,"Q, X or Esc closes the active window.",ATTR_NORMAL);}
static void draw_cursor_settings(void){
    text(22,5,"Cursor settings",ATTR_STATUS);
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
    text(22,16,"Click a setting row or press its shortcut.",ATTR_NORMAL);
    text(22,17,"Changes apply immediately; reboot resets defaults.",ATTR_NORMAL);
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
static int8_t hit_test(uint8_t x,uint8_t y){
    if(y==23){
        if(x>=1&&x<=9)return -1;
        if(x>=12&&x<=19)return 0;
        if(x>=22&&x<=27)return 1;
        if(x>=29&&x<=37)return 2;
        if(x>=40&&x<=44)return 3;
        if(x>=47&&x<=54)return 4;
    }
    if(active_app>=0&&y==4&&x>=72&&x<=76)return -2;
    if(active_app==4&&x>=22&&x<=66){
        if(y>=7&&y<=8)return -4;
        if(y>=9&&y<=10)return -5;
        if(y>=11&&y<=12)return -6;
        if(y>=13&&y<=14)return -7;
    }
    if(active_app<0&&x>=2&&x<=17){
        if(y>=5&&y<=6)return 0;
        if(y>=7&&y<=8)return 1;
        if(y>=9&&y<=10)return 2;
        if(y>=11&&y<=12)return 3;
    }
    if(x>=23&&x<=30&&y>=7&&y<=9)return 0;
    if(x>=33&&x<=40&&y>=7&&y<=9)return 1;
    if(x>=43&&x<=50&&y>=7&&y<=9)return 2;
    if(x>=53&&x<=60&&y>=7&&y<=9)return 1;
    return -3;
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
        else draw_about();
    }
}
void gui_draw(void){cursor_clear();fill(0,0,WIDTH,HEIGHT,' ',ATTR_NORMAL);fill(0,0,WIDTH,1,' ',ATTR_TITLE);text(2,0,"AsterOS",ATTR_TITLE);text(68,0,"Desktop",ATTR_TITLE);border(1,2,18,19);text(3,3,"Applications",ATTR_PANEL);for(uint8_t i=0;i<5;i++)text(3,(uint8_t)(5+i*2),items[i],i==selected?ATTR_SELECT:ATTR_PANEL);draw_desktop_surface();border(20,2,59,19);text(22,3,"Welcome to AsterOS",ATTR_NORMAL);draw_selected_app();draw_taskbar();cursor_draw();}
void gui_init(void){selected=0;active_app=-1;mouse_x=WIDTH*2;mouse_y=HEIGHT*2;cursor_active=0;cursor_visible=1;cursor_speed=1;cursor_shape=0;cursor_color=0;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_last[0]='\0';terminal_reset_input();gui_draw();}
void gui_handle_key(char key){
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
    uint8_t pressed=(uint8_t)(buttons&1);
    static uint8_t previous_buttons;
    cursor_clear();
    mouse_x+=(int16_t)dx*(int16_t)cursor_speed;
    mouse_y-=(int16_t)dy*(int16_t)cursor_speed;
    if(mouse_x<0)mouse_x=0;
    if(mouse_y<0)mouse_y=0;
    if(mouse_x>(WIDTH*4)-1)mouse_x=(WIDTH*4)-1;
    if(mouse_y>(HEIGHT*4)-1)mouse_y=(HEIGHT*4)-1;
    cell_x=(uint8_t)(mouse_x/4);
    cell_y=(uint8_t)(mouse_y/4);
    target=hit_test(cell_x,cell_y);
    if(active_app<0&&target>=0&&target<5&&selected!=(uint8_t)target){
        selected=(uint8_t)target;
        redraw=1;
    }
    if(pressed&&!previous_buttons){
        if(target==-1){
            active_app=-1;
            redraw=1;
        }else if(target==-2){
            active_app=-1;
            terminal_reset_input();
            redraw=1;
        }else if(target==-4){cursor_setting_change(0);redraw=1;
        }else if(target==-5){cursor_setting_change(1);redraw=1;
        }else if(target==-6){cursor_setting_change(2);redraw=1;
        }else if(target==-7){cursor_setting_change(3);redraw=1;
        }else if(target>=0&&target<5){
            selected=(uint8_t)target;
            active_app=target;
            terminal_result=TERM_READY;
            terminal_output[0]='\\0';
            terminal_reset_input();
            redraw=1;
        }
    }
    previous_buttons=pressed;
    if(redraw)gui_draw();
    else cursor_draw();
}
