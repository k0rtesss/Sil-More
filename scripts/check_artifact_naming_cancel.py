#!/usr/bin/env python3
"""Compile real rename_artefact with an instrumented name editor/transaction."""
from pathlib import Path
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'scripts/output/artifact-naming-cancel'
HARNESS=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#define MAX_LEN_ART_NAME 30
#define PR_MISC 1
#define TERM_L_WHITE 1
typedef struct { char name[MAX_LEN_ART_NAME]; unsigned sentinel; } artifact;
static artifact current,backup,original_current,original_backup;
#define smith_a_ptr (&current)
#define smith2_a_ptr (&backup)
static int item=73,turn=421,forge_uses=2,item_count=7,experience=50000;
#define smith_o_ptr (&item)
static struct { char full_name[30]; } options={"Player"};
#define op_ptr (&options)
static struct { int redraw; } player;
#define p_ptr (&player)
static int smith_ui_last_desc_row=-1;
static int smith_ui_content_bottom_row(void){return 10;}
static int smith_ui_desc_col(void){return 0;}
static int smith_ui_term_wid(void){return 80;}
static void Term_erase(int a,int b,int c){(void)a;(void)b;(void)c;}
static void object_desc(char*b,int n,int*o,bool p,int m)
{assert(o==&item);(void)p;(void)m;snprintf(b,n,"Ring");}
static int smith_ui_line_width(int c){(void)c;return 80;}
static void smith_ui_put_fitted(int a,int b,int c,int d,char*e)
{(void)a;(void)b;(void)c;(void)d;(void)e;}
static void Term_gotoxy(int a,int b){(void)a;(void)b;}
static size_t fixture_strlcpy(char*d,const char*s,size_t n)
{size_t size=strlen(s);if(n){size_t count=size<n-1?size:n-1;memcpy(d,s,count);d[count]=0;}return size;}
#define SDL_strlcpy fixture_strlcpy
static int copies,calls,mode;
static void artefact_copy(artifact*d,const artifact*s){copies++;*d=*s;}
static bool askfor_name(char*b,size_t n)
{
    assert(!memcmp(&current,&original_current,sizeof(current)));
    assert(!memcmp(&backup,&original_backup,sizeof(backup)));
    if(!calls) {
        if(strcmp(b,original_backup.name))
            fprintf(stderr,"initial draft truncated: %s / %s\n",b,original_backup.name);
        assert(!strcmp(b,original_backup.name));
    }
    calls++;
    if(mode==0){fixture_strlcpy(b,"discard draft",n);return false;}
    if(mode==1){fixture_strlcpy(b,"Random draft",n);b[0]=0;return false;}
    if(mode==2){b[0]=0;return calls==1;}
    if(mode==4&&calls==1){b[0]=0;return true;}
    fixture_strlcpy(b,"Qa",n);return true;
}
__IMPLEMENTATION__
static void reset(const char*name,int selected_mode)
{
    current=(artifact){.sentinel=0x12345678};backup=(artifact){.sentinel=0x87654321};
    fixture_strlcpy(current.name,"Current design",sizeof(current.name));
    fixture_strlcpy(backup.name,name,sizeof(backup.name));
    assert(strlen(name)<MAX_LEN_ART_NAME);
    original_current=current;original_backup=backup;
    player.redraw=0;calls=copies=0;mode=selected_mode;
}
int main(void)
{
    const char*names[]={"Qa","of Ancient Silver Light","Fëanor's Unbroken Starlight",
        "12345678901234567890123456789"};
    for(int name=0;name<4;name++)for(int scenario=0;scenario<3;scenario++) {
        reset(names[name],scenario);rename_artefact();
        assert(!memcmp(&current,&original_current,sizeof(current)));
        if(memcmp(&backup,&original_backup,sizeof(backup)))
            fprintf(stderr,"cancel changed backup: %s -> %s\n",original_backup.name,backup.name);
        assert(!memcmp(&backup,&original_backup,sizeof(backup)));
        assert(!copies&&!player.redraw&&calls==(scenario==2?2:1));
        assert(item==73&&turn==421&&forge_uses==2&&item_count==7&&experience==50000);
    }
    for(int scenario=3;scenario<5;scenario++) {
    reset(names[1],scenario);rename_artefact();
    assert(!strcmp(current.name,"Qa")&&!strcmp(backup.name,"Qa"));
    assert(copies==1&&calls==(scenario==4?2:1)&&player.redraw==PR_MISC);
    assert(backup.sentinel==original_backup.sentinel&&current.sentinel==original_backup.sentinel);
    assert(item==73&&turn==421&&forge_uses==2&&item_count==7&&experience==50000);
    }
    assert(!strcmp(options.full_name,"Player"));
    puts("Artifact naming: cancel, Random/Clear/cancel, blank retry, full UTF-8 names, accepted commit/accounting: PASS");
}
'''

def main():
    source=(ROOT/'src/cmd/ui/cmd-ui-smithing.c').read_text(encoding='utf-8-sig')
    if '--baseline' in sys.argv:
        source=subprocess.check_output(['git','show','HEAD:src/cmd/ui/cmd-ui-smithing.c'],cwd=ROOT,text=True,encoding='utf-8')
    start=source.index('void rename_artefact(void)');body=source.index('{',start)
    end=body+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    OUT.mkdir(parents=True,exist_ok=True)
    check=OUT/'check.c';check.write_text(HARNESS.replace('__IMPLEMENTATION__',source[start:end]),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join(['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','-Wall','-Wextra',str(check),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=30)

if __name__=='__main__':main()
