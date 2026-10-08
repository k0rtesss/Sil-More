"""Exercise production phone confirmations, labels, details and hintbooks."""
from pathlib import Path
import check_phone_halls_layout as runner

PREFIX = runner.HARNESS[:runner.HARNESS.index('static int width, height;')]
PREFIX = PREFIX.replace('#include "sdl/ui/sdl-halls-screen.c"', '''#include "sdl/ui/sdl-menus.c"
#include "sdl/input/sdl-tooltips.c"
#include "sdl/input/sdl-touch-controls.c"
#include "sdl/ui/sdl-hint-quest-menu.c"
#include "sdl/ui/sdl-pane-menus.c"''')
HARNESS = PREFIX + r'''
static int width,height,keys,last_game_key;
static floor_context_action_kind queued_floor;
static int context_fixture;
static floor_context_action_kind floor_fixture;
static float density;
static int applied,saved;
void __wrap_sdl_apply_config(void){applied++;}
bool __wrap_save_pane_config_to_json(void){saved++;return true;}
float __wrap_SDL_GetDisplayContentScale(SDL_DisplayID d){return density;}
SDL_Rect __wrap_sdl_get_layout_screen_rect(void){return (SDL_Rect){0,0,width,height};}
bool __wrap_sdl_overlay_pane_anchor_rect(int pane,SDL_Rect *r){*r=(SDL_Rect){0,0,width,height};return true;}
bool __wrap_sdl_touch_thumb_current_bounds(SDL_FRect *r){return false;}
bool __wrap_ui_menu_click_handle_choice_action(int c,int a,bool *wake){if(wake)*wake=false;return true;}
errr __wrap_Term_keypress(int k){keys++;last_game_key=k;return 0;}
bool __wrap_sdl_touch_only_device_active(void){return true;}
bool __wrap_player_active_weapon_is_ranged(void){return false;}
bool __wrap_player_quick_throw_available(void){return false;}
void __wrap_sdl_gamepad_send_key(int key,bool bypass){last_game_key=key;}
void __wrap_do_cmd_queue_floor_context_action(floor_context_action_kind kind){queued_floor=kind;}
int __wrap_floor_context_collect_square_actions(bool details,floor_context_action *actions,int capacity){
    if(floor_fixture==FLOOR_CONTEXT_ACTION_NONE)return 0;
    assert(capacity>=2);actions[0]=(floor_context_action){.kind=floor_fixture};
    actions[1]=(floor_context_action){.kind=FLOOR_CONTEXT_ACTION_PACK};return 2;
}
bool __wrap_touch_shortcut_context_action(int binding,bool description,int *key,char *label,size_t size){
    if(binding=='x'){if(key)*key='x';if(label)SDL_strlcpy(label,"Description",size);return true;}
    if(binding!=' ')return false;
    const char *contexts[]={"Confirm","Go Down","Go Up","Open Door","Close Door","Quest Notes","Repair bridge"};
    if(key)*key=' ';if(label)SDL_strlcpy(label,contexts[context_fixture],size);return true;
}
static void inside(SDL_FRect r){
    if(r.x<0||r.y<0||r.x+r.w>width+1||r.y+r.h>height+1)
        fprintf(stderr,"outside %.1f %.1f %.1f %.1f in %dx%d\n",r.x,r.y,r.w,r.h,width,height);
    assert(r.w>0&&r.h>0&&r.x>=-1&&r.y>=-1&&r.x+r.w<=width+1&&r.y+r.h<=height+1);
}
static void snapshot(cptr name){
    char path[160];strnfmt(path,sizeof(path),"%s-%dx%d-%.3f-%s.png",name,width,height,density,config.bigger_font?"on":"off");
    SDL_Surface *p=SDL_RenderReadPixels(g_state.renderer,NULL);assert(p&&IMG_SavePNG(p,path));SDL_DestroySurface(p);
}
static void check_quick_access(void){
    SDL_Rect screen={0,0,width,height};
    SDL_Rect anchor=screen;
    SDL_FRect buttons[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT],panel;
    g_description_overlay.active=false;
    for(int count=9;count<=16;count+=(count==9?2:5))
    for(int preferred_rows=1;preferred_rows<=2;preferred_rows++)
    for(int stretch=0;stretch<2;stretch++)
    for(int edge=0;edge<2;edge++)
    for(int constrained=0;constrained<2;constrained++){
        screen.w=constrained?(int)(width*.8055556f):width;
        anchor=screen;
        config.touch_top_panel_cell_count=count;
        config.touch_top_panel_rows=preferred_rows;
        config.touch_top_panel_size=stretch?SDL_TOUCH_TOP_PANEL_SIZE_STRETCH:SDL_TOUCH_TOP_PANEL_SIZE_DEFAULT;
        const int actual_bindings[]={ 'j','i','y','h',TOUCH_BIND_TOGGLE_TILES,
            'S','l','M','m','o','c','a','b','u','@','p' };
        for(int i=0;i<count;i++)config.touch_top_panel_bindings[i]=actual_bindings[i];
        enum pane_placement where=edge?PLACE_TOP_CENTER:PLACE_BOTTOM_CENTER;
        assert(sdl_touch_top_panel_compute_layout_for_anchor(&screen,&anchor,where,buttons,&panel));
        inside(panel);
        /* Normal controls deliberately keep the historical pixel sizing;
         * check_normal_touch_compat.py compares them with pre-big-text code. */
        assert(sdl_touch_top_panel_reserved_stack_height(&screen)+1>=panel.h);
        if(preferred_rows==1 && count*sdl_touch_top_panel_min_button_size()
           +(count-1)*4*density <= screen.w-2*MIN(18,screen.w*.02f))
            for(int i=1;i<count;i++)assert(buttons[i].y==buttons[0].y);
        for(int i=0;i<count;i++){
            float rounding=config.bigger_font?.01f:1.f;
            inside(buttons[i]);
            if(config.bigger_font)
                assert(buttons[i].w+.01f>=sdl_ui_min_tap_px()&&buttons[i].h+.01f>=sdl_ui_min_tap_px());
            /* Legacy normal mode rounds its panel anchor to integer pixels. */
            assert(buttons[i].x>=panel.x-rounding&&buttons[i].y>=panel.y-rounding);
            assert(buttons[i].x+buttons[i].w<=panel.x+panel.w+rounding);
            assert(buttons[i].y+buttons[i].h<=panel.y+panel.h+rounding);
            for(int j=0;j<i;j++)assert(!SDL_HasRectIntersectionFloat(&buttons[i],&buttons[j]));
            if(i)assert(buttons[i].y>buttons[i-1].y||buttons[i].x>buttons[i-1].x);
        }
        memcpy(g_touch_top_panel_cached_buttons,buttons,sizeof(buttons));
        g_touch_top_panel_cached_layout_valid=true;g_touch_top_panel_cached_generation=g_sdl_present_generation;
        for(int i=0;i<count;i++){
            int slot=-1;
            assert(sdl_touch_top_panel_point_to_slot(buttons[i].x+buttons[i].w/2,buttons[i].y+buttons[i].h/2,&slot));
            assert(slot==i);
        }
        if(config.bigger_font&&width<height&&preferred_rows==1) {
            assert(panel.h>=sdl_ui_min_tap_px()*2);
            if(count<=11 && (!constrained || count<=9)) {
                int rows=1;
                for(int i=1;i<count;i++)if(buttons[i].y!=buttons[i-1].y)rows++;
                assert(rows<=2);
            }
        }
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        for(int i=0;i<count;i++) {
            byte attr;char tile;const char *label;char glyph[32];int tw,th;
            sdl_touch_top_panel_tile_for_binding(config.touch_top_panel_bindings[i],&attr,&tile,&label);
            if(config.bigger_font&&label&&label[0]) {
                const char *fitted=sdl_touch_top_panel_fitting_fallback(&buttons[i],
                    config.touch_top_panel_bindings[i],label,glyph,sizeof(glyph));
                TTF_Font *f=sdl_story_font_for_height_slot(sdl_ui_role_font_px(SDL_UI_FONT_CONTROL),SDL_STORY_FONT_SLOT_MENU);
                sdl_ui_wrapped_text_texture(f,fitted,(int)buttons[i].w-6,g_state.palette[TERM_WHITE],&tw,&th);
                assert(th<=buttons[i].h-4);
            }
            sdl_touch_top_panel_render_icon(&buttons[i],config.touch_top_panel_bindings[i],g_state.palette[TERM_WHITE],false);
        }
        if(preferred_rows==1&&stretch&&edge==0) {
            char name[40];strnfmt(name,sizeof(name),"quick-access%d",count);snapshot(name);
        }
    }
}
static void check_pane_menus(void){
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];
    side_pane_menu_entry sides[MAX_PANE_CONFIGS];SDL_FRect panel;int count=0;
    pane_config_count=2;
    pane_config[0].pane=PANE_LOG;pane_config[0].where=PLACE_BOTTOM;pane_config[0].enabled=true;pane_config[0].rect.rows=4;
    pane_config[1].pane=PANE_ROLLS;pane_config[1].where=PLACE_TOP_RIGHT;pane_config[1].enabled=false;pane_config[1].rect.rows=4;
    g_log_pane_menu.active=true;g_log_pane_menu.target_pane=PANE_LOG;
    g_log_pane_menu.anchor_x=width*.7f;g_log_pane_menu.anchor_y=height*.5f;
    assert(sdl_log_pane_menu_layout(entries,&count,&panel));assert(count==6);inside(panel);
    assert(sdl_log_pane_menu_font_px(PANE_LOG)==sdl_ui_role_font_px(SDL_UI_FONT_CONTROL));
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    sdl_log_pane_menu_render();snapshot("log-context-menu");
    TTF_Font *font=sdl_story_font_for_height_slot(sdl_ui_role_font_px(SDL_UI_FONT_CONTROL),SDL_STORY_FONT_SLOT_MENU);
    TTF_Font *meta=sdl_story_font_for_height_slot(sdl_ui_role_font_px(SDL_UI_FONT_META),SDL_STORY_FONT_SLOT_LOG);
    for(int i=0;i<count;i++){
        SDL_FRect r=entries[i].rect;inside(r);
        assert(r.w>=sdl_ui_min_tap_px()&&r.h>=sdl_ui_min_tap_px());
        assert(sdl_log_pane_menu_index_at(r.x+r.w/2,r.y+r.h/2)==i);
        float required=sdl_pane_menu_text_height(font,entries[i].label,r.w-16*density)
            +4*density+sdl_pane_menu_text_height(meta,entries[i].hint,r.w-16*density)+8*density;
        assert(required<=r.h+.01f);
        SDL_Rect ink={(int)r.x+4,(int)r.y+4,(int)r.w-8,(int)r.h-8};
        SDL_Surface *pixels=SDL_RenderReadPixels(g_state.renderer,&ink);assert(pixels);int bright=0;
        for(int y=0;y<pixels->h;y++)for(int x=0;x<pixels->w;x++){
            Uint8 rr,gg,bb,aa;assert(SDL_ReadSurfacePixel(pixels,x,y,&rr,&gg,&bb,&aa));if(rr>100&&gg>100&&bb>100)bright++;
        }
        SDL_DestroySurface(pixels);assert(bright>20);
        for(int j=0;j<i;j++)assert(!SDL_HasRectIntersectionFloat(&r,&entries[j].rect));
    }
    /* Exercise actual filter actions through pointer up; every ID survives the grid. */
    for(int i=0;i<3;i++){
        g_log_pane_menu.active=true;g_log_pane_menu.target_pane=PANE_LOG;
        g_log_pane_menu.anchor_x=width*.7f;g_log_pane_menu.anchor_y=height*.5f;
        SDL_FRect r=entries[i].rect;
        assert(sdl_log_pane_menu_handle_pointer_down(r.x+r.w/2,r.y+r.h/2,1,false));
        assert(sdl_log_pane_menu_handle_pointer_up(r.x+r.w/2,r.y+r.h/2,1,false));
        assert(g_log_pane_display_pending&&g_log_pane_pending_filter==entries[i].filter);
    }
    for(int i=3;i<6;i++){
        g_log_pane_menu.active=true;g_log_pane_menu.target_pane=PANE_LOG;
        g_log_pane_menu.anchor_x=width*.7f;g_log_pane_menu.anchor_y=height*.5f;
        assert(sdl_log_pane_menu_layout(entries,&count,&panel));
        SDL_FRect r=entries[i].rect;int old_rows=pane_config[0].rect.rows;
        int old_applied=applied,old_saved=saved;
        assert(sdl_log_pane_menu_handle_pointer_down(r.x+r.w/2,r.y+r.h/2,1,false));
        assert(sdl_log_pane_menu_handle_pointer_up(r.x+r.w/2,r.y+r.h/2,1,false));
        assert(applied>old_applied&&saved>old_saved);
        if(i<5)assert(pane_config[0].rect.rows==old_rows+entries[i].row_delta);
        else assert(pane_config[1].enabled);
    }
    enum pane_type panes[]={PANE_INVENTORY,PANE_SUPPLY,PANE_WORN,PANE_INFO,PANE_CHARACTER,PANE_LOG,PANE_MONSTERS,PANE_MAP};
    pane_config_count=N_ELEMENTS(panes);
    for(int i=0;i<pane_config_count;i++){
        pane_config[i].pane=panes[i];pane_config[i].where=PLACE_RIGHT;pane_config[i].enabled=true;pane_config[i].rect.rows=4;
    }
    g_side_pane_menu.active=true;g_side_pane_menu.anchor_x=width*.5f;g_side_pane_menu.anchor_y=height*.5f;
    assert(sdl_side_pane_menu_layout(sides,&count,&panel));assert(count==N_ELEMENTS(panes));inside(panel);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    sdl_side_pane_menu_render();snapshot("side-context-menu");
    for(int i=0;i<count;i++){
        SDL_FRect r=sides[i].rect;inside(r);assert(r.h>=sdl_ui_min_tap_px()&&r.w>=sdl_ui_min_tap_px());
        assert(sdl_side_pane_menu_index_at(r.x+r.w/2,r.y+r.h/2)==i);
        bool old=pane_config[i].enabled;sdl_side_pane_menu_toggle(i);
        assert(pane_config[i].enabled!=old);
    }
    g_side_pane_menu.active=false;g_log_pane_menu.active=false;pane_config_count=0;
}
static void check_context_buttons(void){
    static player_type player;
    player.playing=true;p_ptr=&player;character_dungeon=true;character_icky=0;
    config.touch_thumb_enabled=true;g_description_overlay.active=false;
    for(int scenario=0;scenario<9;scenario++){
        floor_fixture=scenario==0?FLOOR_CONTEXT_ACTION_ITEMS:
            (scenario==1?FLOOR_CONTEXT_ACTION_DETAILS:FLOOR_CONTEXT_ACTION_NONE);
        context_fixture=scenario<2?0:scenario-2;
        touch_thumb_button_set set;sdl_touch_thumb_collect_buttons(&set);
        assert(set.count>0&&set.count<=SDL_TOUCH_THUMB_RUNTIME_CAPACITY);
        g_touch_target_layout=(touch_target_layout_state){.locked=true,.thumb_valid=true,.screen={0,0,width,height}};
        SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
        for(int i=0;i<set.count;i++){
            SDL_FRect r={12*density,12*density+i*(76*density),83*density,
                MAX(48*density,sdl_ui_role_font_px(SDL_UI_FONT_CONTROL)*2.9f+4)};
            g_touch_target_layout.thumb_rects[i]=r;
            char label[128],long_label[64];
            sdl_touch_context_label_for_binding(set.buttons[i].tap_binding,label,sizeof(label));
            if(set.buttons[i].long_binding!=GAMEPAD_BIND_NONE){
                sdl_touch_context_label_for_binding(set.buttons[i].long_binding,long_label,sizeof(long_label));
                SDL_strlcat(label,"\n",sizeof(label));SDL_strlcat(label,long_label,sizeof(label));
            }
            if(scenario<2&&i==0)assert(strcmp(label,config.bigger_font
                ?"Details\nPick Up":"Description\nPick Up")==0);
            if(config.bigger_font) {
                TTF_Font *font=sdl_story_font_for_height_slot(sdl_ui_role_font_px(SDL_UI_FONT_CONTROL),SDL_STORY_FONT_SLOT_MENU);
                int tw,th;sdl_ui_wrapped_text_texture(font,label,(int)r.w-6,g_state.palette[TERM_WHITE],&tw,&th);
                if(th>r.h-4)fprintf(stderr,"Context label %s needs%d in%.1f\n",label,th,r.h);
                assert(th<=r.h-4);
                char words[128];SDL_strlcpy(words,label,sizeof(words));
                for(char *word=strtok(words," \n");word;word=strtok(NULL," \n")){
                    int w,h;assert(TTF_GetStringSize(font,word,0,&w,&h));assert(w<=r.w-6);
                }
            }
            sdl_touch_thumb_render_button(&r,i,false,false);
        }
        for(int i=0;i<set.count;i++){
            SDL_FRect r=g_touch_target_layout.thumb_rects[i];int hit=-1;
            assert(sdl_touch_thumb_point_to_button(r.x+r.w/2,r.y+r.h/2,&hit)&&hit==i);
            last_game_key=0;queued_floor=FLOOR_CONTEXT_ACTION_NONE;
            assert(sdl_touch_thumb_handle_pointer_down(r.x+r.w/2,r.y+r.h/2,false,99));
            assert(sdl_touch_thumb_handle_pointer_up(r.x+r.w/2,r.y+r.h/2,false,99));
            int binding=set.buttons[i].tap_binding;floor_context_action_kind kind;
            if(sdl_touch_thumb_floor_action_from_binding(binding,&kind))assert(queued_floor==kind);
            else assert(last_game_key==(binding==SDL_TOUCH_THUMB_BIND_SPACE_CONTEXT?' ':binding));
            if(set.buttons[i].long_binding!=GAMEPAD_BIND_NONE){
                last_game_key=0;queued_floor=FLOOR_CONTEXT_ACTION_NONE;sdl_touch_thumb_fire(i,true);
                binding=set.buttons[i].long_binding;
                if(sdl_touch_thumb_floor_action_from_binding(binding,&kind))assert(queued_floor==kind);
                else assert(last_game_key==binding);
            }
        }
        if(scenario<2){char name[64];strnfmt(name,sizeof(name),"floor-context%d",scenario);snapshot(name);}
    }
    g_touch_target_layout.locked=false;floor_fixture=FLOOR_CONTEXT_ACTION_NONE;context_fixture=0;
    p_ptr=NULL;character_dungeon=false;
}
static void check(void){
    SDL_FRect panel,prompt,yes,no;
    g_touch_pane_yes_no_prompt_active=true;
    SDL_strlcpy(g_touch_pane_yes_no_prompt_text,
      "Are you sure you want to descend? You cannot return to this floor. Take everything you need before leaving. The stairway leads deeper into the halls and dangerous foes await below. Your journey continues only when you choose Yes. You may choose No to remain here and finish exploring.",sizeof(g_touch_pane_yes_no_prompt_text));
    assert(sdl_touch_pane_yes_no_prompt_layout(&panel,&prompt,&yes,&no));
    inside(panel);inside(prompt);inside(yes);inside(no);
    assert(yes.h>=sdl_ui_min_tap_px()&&yes.w>=sdl_ui_min_tap_px());
    assert(no.h>=sdl_ui_min_tap_px()&&no.w>=sdl_ui_min_tap_px());
    assert(sdl_touch_pane_yes_no_prompt_font_px(5,height)==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    sdl_touch_pane_render_yes_no_prompt();snapshot("confirmation");
    char lines[SDL_TOUCH_YES_NO_MAX_LINES][SDL_TOUCH_YES_NO_LINE_LEN];
    TTF_Font *prompt_font=sdl_story_font_for_height_slot(
        sdl_ui_role_font_px(SDL_UI_FONT_BODY),SDL_STORY_FONT_SLOT_MENU);
    int line_count=sdl_touch_pane_wrap_prompt_lines(g_touch_pane_yes_no_prompt_text,
        prompt_font,prompt.w,lines,SDL_TOUCH_YES_NO_MAX_LINES);
    assert(line_count>0 && strstr(lines[line_count-1],"exploring."));
    int before=keys,pages=g_yes_no_prompt_pages;
    for(int i=0;i<pages;i++){
        sdl_touch_pane_handle_yes_no_prompt_pointer(prompt.x+2,prompt.y+2);
        assert(keys==before&&g_touch_pane_yes_no_prompt_active);
        sdl_touch_pane_render_yes_no_prompt();
    }
    sdl_touch_pane_handle_yes_no_prompt_pointer(yes.x+yes.w/2,yes.y+yes.h/2);
    assert(keys==before+1&&!g_touch_pane_yes_no_prompt_active);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    int px=sdl_ui_role_font_px(SDL_UI_FONT_CONTROL),tw,th;
    TTF_Font *font=sdl_story_font_for_height_slot(px,SDL_STORY_FONT_SLOT_MENU);
    SDL_FRect button={20*density,20*density,120*density,MAX(48*density,px*2.9f+4)};
    sdl_ui_wrapped_text_texture(font,"Wait\nRest",(int)button.w-6,g_state.palette[TERM_WHITE],&tw,&th);
    if(th>button.h-4)fprintf(stderr,"two lines: px%d th%d h%.1f fontheight%d big%d\n",px,th,button.h,TTF_GetFontHeight(font),config.bigger_font); assert(th<=button.h-4);
    sdl_touch_pane_draw_button_text_scaled(&button,"Wait\nRest","z",g_state.palette[TERM_WHITE],0,0);
    snapshot("two-line-button");
    assert(sdl_object_tooltip_font_px()==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    assert(sdl_description_overlay_font_px()==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    byte attrs[4000];char chars[4000];memset(attrs,TERM_WHITE,sizeof(attrs));memset(chars,'x',sizeof(chars));
    g_description_overlay=(description_overlay_state){.active=true,.interactive=true,.attrs=attrs,.chars=chars,.width=40,.height=100,.target_cols=40};
    description_overlay_layout detail;assert(sdl_description_overlay_layout(&detail));
    inside(detail.panel);inside(detail.close_rect);assert(detail.close_rect.h>=sdl_ui_min_tap_px());
    assert(detail.text_y>=detail.close_rect.y+detail.close_rect.h);
    assert(detail.max_scroll>0);g_description_overlay.active=false;
    sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS,"Hints & Quests","Saved knowledge",true,false,0);
    for(int i=0;i<50;i++)sdl_hint_quest_menu_add_block("Remember the long journey: explore carefully and keep supplies ready for dangerous encounters.",TERM_WHITE,0,100+i);
    sdl_hint_quest_menu_add_button(-1,"Previous page",TERM_WHITE);
    sdl_hint_quest_menu_add_button(-2,"Next page",TERM_WHITE);
    sdl_hint_quest_menu_add_button(-3,"Close book",TERM_WHITE);
    sdl_hint_quest_layout book;assert(sdl_hint_quest_layout_compute(&book));
    assert(book.body_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));assert(book.body.h>=sdl_ui_min_tap_px());
    inside(book.title);inside(book.body);inside(book.footer);
    sdl_hint_quest_menu_render();assert(g_hint_quest.max_scroll_y>0);
    for(int i=0;i<g_hint_quest.hit_count;i++)if(g_hint_quest.hits[i].choice<0){
        inside(g_hint_quest.hits[i].rect);assert(g_hint_quest.hits[i].rect.h+0.01f>=sdl_ui_min_tap_px());
    }
    snapshot("hintbook");g_hint_quest.scroll_y=g_hint_quest.max_scroll_y;sdl_hint_quest_menu_render();snapshot("hintbook-end");sdl_hint_quest_menu_hide();
}
int main(int argc,char **argv){
    assert(argc==6);setbuf(stdout,NULL);log_set_quiet(true);
    width=atoi(argv[1]);height=atoi(argv[2]);density=atof(argv[3]);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)&&TTF_Init());
    sdl_config_set_defaults(&config);SDL_strlcpy(config.story_font2,argv[4],sizeof(config.story_font2));SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    g_state.window=SDL_CreateWindow("Overlay phone fixture",width,height,SDL_WINDOW_HIDDEN);assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");assert(g_state.renderer);g_state.system_scale=density;
    for(int i=0;i<16;i++)g_state.palette[i]=(SDL_Color){220,220,220,255};
    /* Legacy normal pixels/geometry are covered by both normal compatibility
     * harnesses; the following checks require big-font reflow. */
    config.bigger_font=true;check();check_quick_access();check_pane_menus();check_context_buttons();
    printf("Overlays %dx%d @%.3f Big font confirmations/buttons/details/hintbook/QuickAccess9,11,16/pane-menu-ink-dispatch/context-labels-hit-dispatch PASS\n",width,height,density);
    return 0;
}
'''


def main():
    # Reuse only the isolated compiler/fixture driver, leaving its test intact.
    runner.HARNESS = HARNESS
    runner.OUT = runner.ROOT / "scripts/output/phone-overlays-check"
    import inspect
    code = inspect.getsource(runner.main)
    code = code.replace('"/src/sdl/ui/sdl-halls-screen.c.obj",',
        '"/src/sdl/ui/sdl-menus.c.obj", "/src/sdl/input/sdl-tooltips.c.obj", '
        '"/src/sdl/input/sdl-touch-controls.c.obj", "/src/sdl/ui/sdl-hint-quest-menu.c.obj", "/src/sdl/ui/sdl-pane-menus.c.obj",')
    code = code.replace('--wrap=Term_keypress",',
        '--wrap=Term_keypress,--wrap=sdl_overlay_pane_anchor_rect,--wrap=sdl_touch_thumb_current_bounds,--wrap=sdl_apply_config,--wrap=save_pane_config_to_json,--wrap=sdl_touch_only_device_active,--wrap=player_active_weapon_is_ranged,--wrap=player_quick_throw_available,--wrap=sdl_gamepad_send_key,--wrap=do_cmd_queue_floor_context_action,--wrap=floor_context_collect_square_actions,--wrap=touch_shortcut_context_action",')
    scope = runner.__dict__.copy()
    exec(code, scope)
    scope['main']()


if __name__ == '__main__':
    main()
