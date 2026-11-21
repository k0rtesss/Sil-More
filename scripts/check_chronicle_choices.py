#!/usr/bin/env python3
"""Replay Chronicle previews, double taps and permanent-change confirmation.

Compiles the real Chronicle builder/state machine, mobile renderer and event
dispatcher. Resource mutations use the real blessing/difficulty functions;
score input, persistence and runtime effects are isolated from player data.
"""
import check_chronicle_scroll as base
from check_phone_welcome_poetry import INIT

CHECKS = r'''
#include "metarun/metarun-stats-ui.c"
static int saves;
errr __wrap_save_metaruns(void) { ++saves; return 0; }
bool __wrap_sync_current_metarun_slot(bool stamp) { return true; }
void __wrap_refresh_current_metar_score(void) {}
u32b __wrap_get_best_run_score_from_highscores(void) { return 0; }
u32b __wrap_compute_blessing_pool(void) { return metar.fallen_score_total; }
int __wrap_blessing_points_available(void)
{ return MAX(0,metar.blessing_points-metar.blessing_points_spent); }
void __wrap_metarun_apply_runtime_effects(void) {}

static story_book_sdl_state choice_state;
static void render_choices(void)
{
    SDL_Rect canvas={0,0,fixture_width,fixture_height};
    g_sdl_narrative_portrait_rendering=fixture_height>fixture_width;
    sdl_character_sheet_screen_render_canvas(&canvas);
    g_sdl_narrative_portrait_rendering=false;
}
static int put_text(char *pool,int *used,cptr text)
{
    int offset=*used; size_t length=strlen(text)+1;
    SDL_memcpy(pool+offset,text,length); *used+=(int)length; return offset;
}
static void setup_choices(void)
{
    static curse_type curses[4]; static major_blessing_type majors[2];
    static runtype_type types[3];
    static char names[512],texts[4096],major_names[512],major_texts[4096];
    int name_used=1,text_used=1,major_name_used=1,major_text_used=1;
    memset(curses,0,sizeof(curses)); memset(majors,0,sizeof(majors)); memset(types,0,sizeof(types));
    z_info->cu_max=4; z_info->mb_max=2; z_info->rt_max=3;
    cu_info=curses; cu_name=names; cu_text=texts;
    mb_info=majors; mb_name=major_names; mb_text=major_texts; runtype_info=types;
    for(int i=0;i<4;i++) {
        curses[i].name=put_text(names,&name_used,"Shadow of Doubt");
        curses[i].blessing_name=put_text(names,&name_used,i==1?"Mercy of the West":"Hope of the Eldar");
        curses[i].text=put_text(texts,&text_used,"A shadow follows the heroes of this tale.");
        curses[i].power=put_text(texts,&text_used,"CURSE MECHANICS: -1 Will for each stack.");
        curses[i].blessing_text=put_text(texts,&text_used,"BLESSING LORE: The West remembers every fallen hero.");
        curses[i].blessing_power=put_text(texts,&text_used,"BLESSING MECHANICS: +1 Will for each blessing stack.");
        curses[i].max_stacks=10; curses[i].max_blessing_stacks=4; curses[i].weight=1;
    }
    for(int i=0;i<2;i++) {
        majors[i].name=put_text(major_names,&major_name_used,i?"Unattainable Gift":"Light of the West");
        majors[i].detail_desc=put_text(major_texts,&major_text_used,
            "MAJOR MECHANICS: This covenant protects every future hero in the tale.");
        majors[i].cost=i?25:3;
    }
    for(int i=0;i<3;i++) {
        SDL_strlcpy(types[i].name,i?"Echoes (Hard)":"Echoes (Normal)",sizeof(types[i].name));
        types[i].win_con=15; types[i].colour=TERM_L_GREEN;
        types[i].blessing_threshold_modes[METARUN_BLESSING_THRESHOLD_NORMAL]=500;
        types[i].blessing_threshold_modes[METARUN_BLESSING_THRESHOLD_EASIER]=400;
        types[i].blessing_threshold_modes[METARUN_BLESSING_THRESHOLD_HARDER]=650;
        if(i) {types[i].start_curses=1;types[i].curse_stacks[0]=2;}
    }
    memset(&metar,0,sizeof(metar)); metar.id=1;
    metar.curse_stacks[0]=1; metar.curses_seen=1;
    metar.blessing_points=10; metar.fallen_score_total=5000;
    metar.pending_blessing_count=3;
    for(int i=0;i<3;i++) metar.pending_blessing_choices[i]=i+1;
    choice_state=(story_book_sdl_state){-1,0,0,0,-1,-1}; saves=0;
    run_mode_set_current(RUN_MODE_STORY);
    sdl_character_sheet_screen_hide(); ui_menu_click_clear(); Term_flush();
}
static void rebuild_choices(int page,int choice)
{
    story_book_sdl_build(true,&choice_state,page);
    if(story_book_sdl_change_action(choice) && choice_state.pending_action<0)
        sdl_character_sheet_screen_anchor_book_choice(choice);
    render_choices();
}
static bool has_paragraph(cptr text)
{
    for(int i=0;i<g_sdl_character_sheet_screen.narrative_para_count;i++)
        if(strstr(g_sdl_character_sheet_screen.narrative_paras[i],text)) return true;
    return false;
}
static SDL_FRect find_choice(int choice)
{
    for(int attempt=0;attempt<250;attempt++) {
        for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
            sdl_character_sheet_hit *hit=&g_sdl_character_sheet_screen.hits[i];
            if(hit->choice==choice && hit->rect.h>=12*fixture_density) return hit->rect;
        }
        if(!sdl_character_sheet_screen_scroll_book(1)) {
            fprintf(stderr,"Missing choice %d on page %d, selected %d pending %d, offset %d/%d\n",
                choice,sdl_character_sheet_screen_select_page(),choice_state.selected_action,
                choice_state.pending_action,g_sdl_character_sheet_screen.sheet_scroll,
                g_sdl_character_sheet_screen.sheet_scroll_max);
            for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++)
                fprintf(stderr,"hit %d height %.1f\n",g_sdl_character_sheet_screen.hits[i].choice,
                    g_sdl_character_sheet_screen.hits[i].rect.h);
            fixture_assert(false);
        }
        render_choices();
    }
    fixture_assert(false); return (SDL_FRect){0};
}
static void finger_choice(Uint32 type,float x,float y)
{
    SDL_Event ev={0};ev.type=type;ev.tfinger.windowID=SDL_GetWindowID(g_state.window);
    ev.tfinger.touchID=1;ev.tfinger.fingerID=7;
    ev.tfinger.x=x/fixture_width;ev.tfinger.y=y/fixture_height;
    sdl_handle_event(&g_state,&ev);
}
static void tap_book_hit_at(int choice,SDL_FRect hit)
{
    ui_menu_click_begin();
    float x=hit.x+hit.w/2,y=hit.y+hit.h/2;
    finger_choice(SDL_EVENT_FINGER_DOWN,x,y);finger_choice(SDL_EVENT_FINGER_UP,x,y);
    int picked=-1,action=-1;char key=0;
    fixture_assert(ui_menu_click_take_action(&picked,&action));
    fixture_assert(picked==choice && action==UI_MENU_CLICK_PRIMARY);
    fixture_assert(Term_inkey(&key,false,true)==0);
    fixture_assert(Term_inkey(&key,false,true)!=0);
    ui_menu_click_clear();
}
static SDL_FRect tap_book_hit(int choice)
{
    SDL_FRect hit=find_choice(choice);
    tap_book_hit_at(choice,hit);
    return hit;
}
static SDL_FRect tap_choice(int choice)
{
    SDL_FRect hit=tap_book_hit(choice);
    int page=sdl_character_sheet_screen_select_page();
    fixture_assert(story_book_sdl_handle_change(&choice_state,choice,UI_MENU_CLICK_PRIMARY));
    rebuild_choices(page,choice);
    return hit;
}
static void check_change(int choice,cptr description)
{
    strnfmt(fixture_id,sizeof(fixture_id),"choice-%d-big-%d",choice,config.bigger_font);
    setup_choices();
    int page=choice>=STORY_BOOK_THRESHOLD_BASE?STORY_BOOK_DIFFICULTY:STORY_BOOK_BLESSINGS;
    rebuild_choices(page,-1);
    metarun original=metar;
    tap_choice(choice);
    fixture_assert(choice_state.selected_action==choice && choice_state.pending_action==-1);
    fixture_assert(memcmp(&original,&metar,sizeof(metar))==0);
    fixture_assert(has_paragraph(description));
    SDL_FRect after=find_choice(choice);
    SDL_FRect reading=g_sdl_character_sheet_screen.select_scroll_rect;
    fixture_assert(after.y<reading.y+reading.h*.35f);
    char name[100];strnfmt(name,sizeof(name),"preview-%d",choice);capture(name);
    /* Scrolling the expanded description must not count as another tap. */
    SDL_FRect view=g_sdl_character_sheet_screen.select_scroll_rect;
    float x=view.x+view.w/2,y=view.y+view.h*.8f;
    ui_menu_click_begin();
    finger_choice(SDL_EVENT_FINGER_DOWN,x,y);
    finger_choice(SDL_EVENT_FINGER_MOTION,x,y-view.h*.5f);
    finger_choice(SDL_EVENT_FINGER_UP,x,y-view.h*.5f);
    fixture_assert(!ui_menu_click_has_pending());
    char key=0;fixture_assert(Term_inkey(&key,false,true)!=0);
    ui_menu_click_clear(); render_choices();
    fixture_assert(choice_state.pending_action==-1);
    while(sdl_character_sheet_screen_scroll_book(-1)) render_choices();
    tap_choice(choice);
    fixture_assert(choice_state.pending_action==choice);
    fixture_assert(memcmp(&original,&metar,sizeof(metar))==0);
    fixture_assert(has_paragraph(description));
    tap_choice(STORY_BOOK_ACTION_CANCEL);
    fixture_assert(choice_state.selected_action==-1 && choice_state.pending_action==-1);
    fixture_assert(memcmp(&original,&metar,sizeof(metar))==0);
    tap_choice(choice);fixture_assert(choice_state.pending_action==-1);
    tap_choice(choice);fixture_assert(choice_state.pending_action==choice);
    strnfmt(name,sizeof(name),"confirmation-%d",choice);capture(name);
    tap_choice(STORY_BOOK_ACTION_CONFIRM);
    fixture_assert(choice_state.pending_action==-1 && choice_state.selected_action==-1);
    fixture_assert(saves>0);
    if(choice==STORY_BOOK_MINOR_BASE+1) {
        fixture_assert(metar.curse_stacks[1]==-1 && metar.blessing_points_spent==1);
    } else if(choice==STORY_BOOK_MAJOR_BASE) {
        fixture_assert(metar.major_blessings==1 && metar.blessing_points_spent==3);
    } else if(choice==STORY_BOOK_REMOVE_CURSE_BASE) {
        fixture_assert(metar.curse_stacks[0]==0 && metar.blessing_points_spent==1);
    } else if(choice>=STORY_BOOK_DIFFICULTY_BASE) {
        fixture_assert(metar.type==1 && metar.max_difficulty_reached==1);
    } else {
        fixture_assert(metarun_get_threshold_mode(&metar)==METARUN_BLESSING_THRESHOLD_EASIER);
    }
    metarun committed=metar;
    fixture_assert(story_book_sdl_handle_change(&choice_state,STORY_BOOK_ACTION_CONFIRM,UI_MENU_CLICK_PRIMARY));
    fixture_assert(memcmp(&committed,&metar,sizeof(metar))==0);
}
static void check_guards(void)
{
    setup_choices();
    int first=STORY_BOOK_MINOR_BASE+1,second=STORY_BOOK_MINOR_BASE+2;
    metarun original=metar;
    fixture_assert(!story_book_sdl_handle_change(&choice_state,first,UI_MENU_CLICK_HOVER));
    fixture_assert(choice_state.selected_action==-1);
    story_book_sdl_handle_change(&choice_state,first,UI_MENU_CLICK_PRIMARY);
    story_book_sdl_handle_change(&choice_state,second,UI_MENU_CLICK_PRIMARY);
    story_book_sdl_handle_change(&choice_state,first,UI_MENU_CLICK_PRIMARY);
    fixture_assert(choice_state.pending_action==-1);
    story_book_sdl_handle_change(&choice_state,first,UI_MENU_CLICK_SECONDARY);
    fixture_assert(choice_state.pending_action==-1);
    story_book_sdl_handle_change(&choice_state,STORY_BOOK_ACTION_CONFIRM,UI_MENU_CLICK_PRIMARY);
    fixture_assert(memcmp(&original,&metar,sizeof(metar))==0);
    rebuild_choices(STORY_BOOK_BLESSINGS,-1);
    tap_choice(STORY_BOOK_MAJOR_BASE+1);
    fixture_assert(has_paragraph("MAJOR MECHANICS"));
    tap_choice(STORY_BOOK_MAJOR_BASE+1);
    fixture_assert(choice_state.pending_action==-1);
    fixture_assert(has_paragraph("Not enough blessing points"));
    fixture_assert(memcmp(&original,&metar,sizeof(metar))==0);
    run_mode_set_current(RUN_MODE_BLITZ);
    fixture_assert(!story_book_sdl_handle_change(&choice_state,first,UI_MENU_CLICK_PRIMARY));
    fixture_assert(memcmp(&original,&metar,sizeof(metar))==0);
    run_mode_set_current(RUN_MODE_STORY);
}
static void check_effect_hover(int choice,cptr expected,cptr hidden)
{
    SDL_FRect hit=find_choice(choice);
    fixture_assert(sdl_character_sheet_screen_handle_pointer_motion(
        hit.x+hit.w/2,hit.y+hit.h/2));
    render_choices();
    cptr description=sdl_char_sheet_hover_desc(NULL,NULL);
    fixture_assert(strstr(description,expected));
    if(hidden) fixture_assert(!strstr(description,hidden));
    fixture_assert(g_sdl_char_sheet_hover_tooltip_rect.w>0);
    sdl_character_sheet_screen_handle_pointer_motion(-1,-1);
    render_choices();
    fixture_assert(!g_sdl_char_sheet_hover_tooltip_rect.w);
}
/* Drive touch releases through the Chronicle's real input loop. A rebuild
 * used to erase the popup between the release and the next presented frame. */
static int inspect_step, inspected_choice, inspected_scroll;
void __wrap_screen_push_supporting_panes_hidden(void) {}
void __wrap_screen_pop_supporting_panes_hidden(void) {}
static errr fixture_term_extra(int action,int value) { return 0; }
char __wrap_metarun_inkey_hidden(void)
{
    if (inspect_step == 0) {
        sdl_character_sheet_screen_set_book_page(STORY_BOOK_CURSES);
        render_choices();
    } else {
        for (int frame = 0; frame < 3; ++frame) {
            render_choices();
            fixture_assert(g_sdl_char_sheet_hover_tooltip_choice == inspected_choice);
            fixture_assert(g_sdl_char_sheet_hover_tooltip_rect.w > 0);
            fixture_assert(g_sdl_character_sheet_screen.sheet_scroll ==
                MIN(inspected_scroll,g_sdl_character_sheet_screen.sheet_scroll_max));
            cptr desc = sdl_char_sheet_hover_desc(NULL,NULL);
            fixture_assert(strstr(desc,"A shadow follows"));
            fixture_assert((strstr(desc,"CURSE MECHANICS") != NULL) == (inspect_step < 3));
        }
        SDL_FRect box = g_sdl_char_sheet_hover_tooltip_rect;
        float x = box.x + box.w / 2, y = box.y + box.h / 2;
        finger_choice(SDL_EVENT_FINGER_DOWN,x,y);
        finger_choice(SDL_EVENT_FINGER_UP,x,y);
        render_choices();
        fixture_assert(!g_sdl_char_sheet_hover_tooltip_rect.w);
        fixture_assert(!ui_menu_click_has_pending());
        if (inspect_step == 3) return ESCAPE;
    }

    /* Tap another row, hold the same row, then hold an undiscovered curse. */
    inspected_choice = STORY_BOOK_CURSE_BASE + (inspect_step < 2 ? 1 : 3);
    SDL_FRect hit = find_choice(inspected_choice);
    float x = hit.x + hit.w / 2, y = hit.y + hit.h / 2;
    inspected_scroll = g_sdl_character_sheet_screen.sheet_scroll;
    finger_choice(SDL_EVENT_FINGER_DOWN,x,y);
    if (inspect_step > 0 && g_sdl_character_sheet_screen.touch_press.active)
        g_sdl_character_sheet_screen.touch_press.start_time -= 700000000ULL;
    finger_choice(SDL_EVENT_FINGER_UP,x,y);
    char key;
    fixture_assert(ui_menu_click_has_pending() && Term_inkey(&key,false,true) == 0);
    ++inspect_step;
    return key;
}
static void check_curse_touch_inspection(void)
{
    setup_choices();
    metar.curses_seen = 7;
    metar.curse_stacks[3] = 1;
    inspect_step = 0;
    u64b blessings = 0;
    fixture_assert(story_book_show_sdl(true,&blessings));
    fixture_assert(inspect_step == 3 && saves == 0 && blessings == 0);
}
static void check_statistics_and_descriptions(void)
{
    setup_choices();
    metar.blessing_points=0;
    metar.curse_stacks[1]=-1;
    metar.major_blessings=1;
    rebuild_choices(STORY_BOOK_STATISTICS,-1);
    SDL_FRect quit_button=tap_book_hit(SDL_SELECT_CLICK_BOOK_QUIT);
    SDL_FRect create_button=find_choice(SDL_SELECT_CLICK_CLOSE);
    fixture_assert(quit_button.x+quit_button.w<=create_button.x);
    SDL_FRect lamp=tap_book_hit(STORY_BOOK_EASY_MODE);
    bool found_help=false;
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        const sdl_character_sheet_hit* hit=&g_sdl_character_sheet_screen.hits[i];
        if(hit->choice!=STORY_BOOK_EASY_MODE || hit->rect.x<lamp.x+lamp.w) continue;
        tap_book_hit_at(STORY_BOOK_EASY_MODE,hit->rect);
        found_help=true;
        break;
    }
    fixture_assert(found_help);
    fixture_assert(!g_sdl_character_sheet_screen.narrative_lamp_active);

    rebuild_choices(STORY_BOOK_BLESSINGS,-1);
    check_effect_hover(STORY_BOOK_OWNED_MINOR_BASE+1,"BLESSING MECHANICS",NULL);
    check_effect_hover(STORY_BOOK_OWNED_MAJOR_BASE,"MAJOR MECHANICS",NULL);
    rebuild_choices(STORY_BOOK_CURSES,-1);
    check_effect_hover(STORY_BOOK_CURSE_BASE,"CURSE MECHANICS",NULL);
    metar.curses_seen=0;
    rebuild_choices(STORY_BOOK_CURSES,-1);
    check_effect_hover(STORY_BOOK_CURSE_BASE,"A shadow follows","CURSE MECHANICS");
    fixture_assert(saves==0);
}
'''

base.OUT = base.ROOT / "scripts/output/chronicle-choices"
base.EXTRA_EXCLUDED = ("/src/metarun/metarun-stats-ui.c.obj",)
base.EXTRA_WRAPS = (
    "save_metaruns", "sync_current_metarun_slot", "refresh_current_metar_score",
    "get_best_run_score_from_highscores", "compute_blessing_pool",
    "blessing_points_available", "metarun_apply_runtime_effects",
    "metarun_inkey_hidden", "screen_push_supporting_panes_hidden",
    "screen_pop_supporting_panes_hidden",
)
# SDL's private header already supplies externs.h (which has no include guard).
internal = (base.ROOT / "src/metarun/metarun-internal.h").read_text(encoding="utf-8")
internal = internal.replace('#include "externs.h"', "").replace(
    '#include "../metarun.h"', '#include "metarun.h"')
base.HARNESS = base.PREFIX + internal + CHECKS + INIT + r'''
    view->t.xtra_hook=fixture_term_extra;
    g_sdl_blocking_key_wait=true;fixture.active=false;
    g_state.palette[TERM_L_BLUE]=(SDL_Color){0,230,230,255};
    g_state.palette[TERM_L_GREEN]=(SDL_Color){0,255,0,255};
    g_state.palette[TERM_L_RED]=(SDL_Color){255,95,95,255};
    g_state.palette[TERM_YELLOW]=(SDL_Color){255,255,0,255};
    for(int big=0;big<2;big++) {
        config.bigger_font=big;
        for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) config.menu_bigger_font[menu]=big;
        check_change(STORY_BOOK_MINOR_BASE+1,"BLESSING MECHANICS");
        check_change(STORY_BOOK_MAJOR_BASE,"MAJOR MECHANICS");
        check_change(STORY_BOOK_REMOVE_CURSE_BASE,"CURSE MECHANICS");
        check_change(STORY_BOOK_THRESHOLD_BASE+METARUN_BLESSING_THRESHOLD_EASIER,"Earn one blessing point");
        check_change(STORY_BOOK_DIFFICULTY_BASE+1,"A difficulty increase is permanent");
        check_guards();
        check_statistics_and_descriptions();
        check_curse_touch_inspection();
    }
    printf("Chronicle choices %dx%d @%.3f: descriptions, two taps, explicit confirmation, Cancel, resource safety PASS\n",
        fixture_width,fixture_height,fixture_density);
    sdl_character_sheet_screen_hide();sdl_ui_text_cache_clear();sdl_story_font_cache_clear();term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer);SDL_DestroyWindow(g_state.window);TTF_Quit();SDL_Quit();
}
'''

if __name__ == "__main__":
    base.main()
