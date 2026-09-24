#include "gui.h"
#define VGA ((volatile uint16_t*)0xB8000)
#define WIDTH 80
#define HEIGHT 25
#define ATTR_NORMAL 0x07
#define ATTR_TITLE 0x1F
#define ATTR_PANEL 0x70
#define ATTR_STATUS 0x1E
#define ATTR_SELECT 0x71
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
static char terminal_buffer[TERM_INPUT_MAX + 1];
static char terminal_output[TERM_INPUT_MAX + 1];
static char terminal_last[TERM_INPUT_MAX + 1];
static const char *items[] = {"Terminal", "Files", "Programs", "About"};

enum { TERM_READY = 0, TERM_HELP, TERM_APPS, TERM_INFO, TERM_VER, TERM_ECHO, TERM_PWD, TERM_HISTORY, TERM_UNAME, TERM_UNKNOWN };

static void cell(uint8_t x,uint8_t y,char c,uint8_t a){if(x<WIDTH&&y<HEIGHT)VGA[(uint16_t)y*WIDTH+x]=((uint16_t)a<<8)|(uint8_t)c;}
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
            text(22,9,"Terminal  Files  Programs  About",ATTR_NORMAL);
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
static void draw_about(void){text(22,5,"About AsterOS",ATTR_STATUS);text(22,7,"32-bit x86 experimental operating system",ATTR_NORMAL);text(22,8,"GUI shell over the CCP/BDOS direction",ATTR_NORMAL);text(22,10,"Enter opens the selected app.",ATTR_NORMAL);text(22,11,"Arrow keys or W/S navigate.",ATTR_NORMAL);text(22,12,"1-4 or T/F/P/A launches apps directly.",ATTR_NORMAL);text(22,13,"Q, X or Esc closes the active window.",ATTR_NORMAL);}
static void draw_selected_app(void){
    fill(21,4,57,18,' ',ATTR_NORMAL);
    if(active_app<0){
        text(22,5,"Select an application",ATTR_STATUS);
        text(22,7,"Use W/S or arrow keys and press Enter.",ATTR_NORMAL);
        text(22,8,"Press 1-4 or T/F/P/A to launch an app.",ATTR_NORMAL);
        text(22,10,"Desktop applications",ATTR_STATUS);
        text(22,12,"[T] Terminal   [F] Files",ATTR_NORMAL);
        text(22,13,"[P] Programs  [A] About",ATTR_NORMAL);
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
void gui_draw(void){fill(0,0,WIDTH,HEIGHT,' ',ATTR_NORMAL);fill(0,0,WIDTH,1,' ',ATTR_TITLE);text(2,0,"AsterOS",ATTR_TITLE);text(68,0,"GUI Shell",ATTR_TITLE);border(1,2,18,19);text(3,3,"Applications",ATTR_PANEL);for(uint8_t i=0;i<4;i++)text(3,(uint8_t)(5+i*2),items[i],i==selected?ATTR_SELECT:ATTR_PANEL);border(20,2,59,19);text(22,3,"Welcome to AsterOS",ATTR_NORMAL);draw_selected_app();fill(0,23,WIDTH,2,' ',ATTR_STATUS);text(2,23,active_app<0?"Enter Open   W/S/Arrows Navigate":"Tab Switch   Q/X/Esc Close   Type commands   Enter",ATTR_STATUS);}
void gui_init(void){selected=0;active_app=-1;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_last[0]='\0';terminal_reset_input();gui_draw();}
void gui_handle_key(char key){
    if(active_app<0){
        if((uint8_t)key==KEY_UP||key=='w'||key=='W'){if(selected==0)selected=3;else--selected;gui_draw();}
        else if((uint8_t)key==KEY_DOWN||key=='s'||key=='S'){selected=(uint8_t)((selected+1)%4);gui_draw();}
        else if(key>='1'&&key<='4'){selected=(uint8_t)(key-'1');active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();}
        else if(key=='t'||key=='T'||key=='f'||key=='F'||key=='p'||key=='P'||key=='a'||key=='A'){
            if(key=='t'||key=='T')selected=0;
            else if(key=='f'||key=='F')selected=1;
            else if(key=='p'||key=='P')selected=2;
            else selected=3;
            active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();
        }
        else if(key=='\r'||key=='\n'){active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();}
        return;
    }
    if(key==KEY_TAB){selected=(uint8_t)((selected+1)%4);active_app=(int8_t)selected;terminal_result=TERM_READY;terminal_output[0]='\0';terminal_reset_input();gui_draw();return;}
    if(key=='q'||key=='Q'||key=='x'||key=='X'||key==27){active_app=-1;terminal_reset_input();gui_draw();return;}
    if(active_app!=0)return;
    if(key=='\b'){
        if(terminal_length){terminal_length--;terminal_buffer[terminal_length]='\0';gui_draw();}
    }else if(key=='\r'||key=='\n'){
        terminal_execute();gui_draw();
    }else if(key>=32&&key<=126){
        if(terminal_length<TERM_INPUT_MAX){terminal_buffer[terminal_length++]=key;terminal_buffer[terminal_length]='\0';gui_draw();}
    }
}
