#!/usr/bin/env python3
"""Run the production text editor with byte input, pointer controls and UTF-8 limits."""
from pathlib import Path
import os
import re
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'scripts/output/string-panel-utf8'
HARNESS=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
typedef const char* cptr;
#define ESCAPE 27
#define SDL_TOUCH_MENU_CATEGORY_OTHER 0
#define UI_MENU_CLICK_WAKE_KEY 31
#define UI_MENU_CLICK_PRIMARY 1
#define UI_MENU_CLICK_HOVER 2
static bool hide_cursor;
static int depth,begins,ends,reopens,draws,clears,click_clears;
static size_t capacity;
__UTF8__
static bool valid(cptr s)
{
    size_t n=strlen(s);
    for(size_t i=0;i<n;) {
        int size=utf8_sequence_len_n(s+i,(int)(n-i));
        if(size<1||((unsigned char)s[i]>=128&&size==1))return false;
        i+=(size_t)size;
    }
    return true;
}
static size_t fixture_strlcpy(char*d,cptr s,size_t n)
{size_t size=strlen(s);if(n){size_t count=size<n-1?size:n-1;memcpy(d,s,count);d[count]=0;}return size;}
#define SDL_strlcpy fixture_strlcpy
typedef struct {int key,choice,action;} event;
static event events[256],current_event;
static int count,next;
static void key(int k){assert(count<256);events[count++]=(event){k,0,0};}
static void text(cptr s){while(*s)key((unsigned char)*s++);}
static void click(int choice){events[count++]=(event){31,choice,1};}
static void hover(void){events[count++]=(event){31,2001,2};}
static void message_flush(void){}
static void inkey_prompt_input_begin(void){depth++;}
static void inkey_prompt_input_end(void){depth--;}
static void sdl_text_input_begin(void){begins++;}
static void sdl_text_input_end(void){ends++;}
static void sdl_text_input_reopen(void){reopens++;}
static void string_panel_draw(cptr title,cptr value,int category,bool random,bool selected)
{(void)title;(void)category;(void)random;(void)selected;draws++;assert(hide_cursor&&depth==1);
 if(!valid(value))fprintf(stderr,"invalid draft at draw%d bytes%zu\n",draws,strlen(value));
 assert(valid(value)&&strlen(value)<capacity);}
static int inkey(void){assert(next<count);current_event=events[next++];return current_event.key;}
static bool ui_menu_click_take_action(int*choice,int*action)
{if(!current_event.choice)return false;*choice=current_event.choice;*action=current_event.action;return true;}
static void make_random_name(char*b,size_t n){fixture_strlcpy(b,"Random",n);}
static void sdl_question_menu_clear(void){clears++;}
static void ui_menu_click_clear(void){click_clears++;}
static void Term_fresh(void){}
__CONSTANTS__
__HELPER__
__EDITOR__
static void start(void)
{count=next=0;current_event=(event){0};begins=ends=reopens=draws=clears=click_clears=depth=0;hide_cursor=false;}
static void finish(size_t n,cptr initial,cptr expected,bool accepted)
{
    unsigned char storage[128];memset(storage,0xcc,sizeof(storage));
    char*buf=(char*)storage;fixture_strlcpy(buf,initial,n);capacity=n;
    bool result=get_string_panel_aux("Name:",buf,n,true);
    assert(result==accepted&&!hide_cursor&&!depth);
    if(n) {
        if(strcmp(buf,expected))fprintf(stderr,"capacity%zu expected[%s] actual[%s]\n",n,expected,buf);
        assert(!strcmp(buf,expected)&&valid(buf));
        assert(begins==1&&ends==1&&clears==1&&click_clears==1&&next==count);
    } else assert(!begins&&!ends&&!draws);
    for(size_t i=n;i<sizeof(storage);i++)assert(storage[i]==0xcc);
}
int main(void)
{
    cptr chars[]={"é","€","😀"};
    for(int i=0;i<3;i++)for(size_t n=1;n<=8;n++) {
        start();text(chars[i]);key('\r');finish(n,"",strlen(chars[i])<n?chars[i]:"",true);
    }
    start();text("AAAAAAAAAAAAAAAAAAAAAAAAAAAAé");key('\r');finish(30,"","AAAAAAAAAAAAAAAAAAAAAAAAAAAA",true);
    start();text("AAAAAAAAAAAAAAAAAAAAAAAAAAAé");key('\r');finish(30,"","AAAAAAAAAAAAAAAAAAAAAAAAAAAé",true);
    start();text("AAAAAAAAAAAAAAAAAAAAAAAAAAAAéB");key('\r');finish(30,"","AAAAAAAAAAAAAAAAAAAAAAAAAAAAB",true);
    start();text("é");key('\r');finish(30,"old default","é",true);
    start();text("é");key('\r');finish(2,"A","A",true);
    start();text("éB");key('\r');finish(2,"A","B",true);
    start();text("Aé😀");key('\b');key('\r');finish(30,"","Aé",true);
    start();key('\b');key('\r');finish(30,"old default","",true);
    start();key(0xe2);text("x");key('\r');finish(30,"","x",true);
    start();key(0x80);key(0xc0);key(0xe0);key(0x80);key(0xaf);text("x");key('\r');finish(30,"","x",true);
    start();key(0xed);key(0xa0);key(0x80);key(0xf4);key(0x90);key(0x80);key(0x80);key('\r');finish(30,"","",true);
    start();key(0xe2);key(0x82);key('\r');finish(30,"old","old",true);
    start();key(0xe2);key(ESCAPE);finish(30,"old","old",false);
    start();key(0xc3);click(2003);key(0xa9);text("x");click(2002);finish(30,"old","x",true);
    start();key(0xe2);click(2005);key(0x82);text("x");click(2002);finish(30,"old","x",true);
    start();key(0xe2);click(2001);key(0x82);text("x");click(2002);finish(30,"old","oldx",true);assert(reopens==1);
    start();key(0xe2);click(2002);finish(30,"old","old",true);
    start();key(0xe2);click(2004);finish(30,"old","old",false);
    start();key(0xe2);key('\t');key('\r');finish(30,"old","Random",true);
    start();key(0xc3);hover();key(0xa9);key('\r');finish(30,"","é",true);
    start();key('\r');finish(30,"12345678901234567890123456789","12345678901234567890123456789",true);
    start();finish(0,"","",false);
    puts("String editor: whole UTF-8 capacity, valid intermediate drafts, selection/Backspace, malformed input, pointer/keyboard controls: PASS");
}
'''

def function(source,signature):
    start=source.index(signature);body=source.index('{',start);end=body+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

def main():
    prompt=(ROOT/'src/support/prompt.c').read_text(encoding='utf-8-sig')
    if '--baseline' in sys.argv:
        prompt=subprocess.check_output(['git','show','HEAD:src/support/prompt.c'],cwd=ROOT,text=True,encoding='utf-8')
    marker='typedef struct string_panel_utf8_state'
    helper=prompt[prompt.index(marker):prompt.index('static void string_panel_add_action',prompt.index(marker))] if marker in prompt else ''
    editor=function(prompt[prompt.rindex('static bool get_string_panel_aux'):],'static bool get_string_panel_aux')
    utf8=(ROOT/'src/support/utf8.c').read_text(encoding='utf-8-sig')
    utf8=utf8[utf8.index('static bool utf8_is_continuation'):utf8.index('int utf8_sequence_len(cptr')]
    constants='\n'.join(re.findall(r'^#define STRING_PANEL_CLICK_\w+\s+\d+',prompt,re.M))
    code=HARNESS.replace('__UTF8__',utf8).replace('__CONSTANTS__',constants).replace('__HELPER__',helper).replace('__EDITOR__',editor)
    OUT.mkdir(parents=True,exist_ok=True);check=OUT/'check.c';check.write_text(code,encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join(['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','-Wall','-Wextra',str(check),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=30)

if __name__=='__main__':main()
