#!/usr/bin/env python3
"""Check production ice projections and SDL pixels in an isolated software map.

Build with build-incremental.ps1 first. Reuses the fixture renderer harness;
does not open player saves or change user configuration.
"""
from pathlib import Path
import check_idle_animation as idle

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/ice-check"

TESTS = r'''
#include "spell/spell-projection-internal.h"
#include "spell/spell-projection.h"
#include "player/player-upkeep-internal.h"

static void ice_map(void) {
    character_dungeon=false;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=32;
    p_ptr->py=p_ptr->px=15; p_ptr->wy=p_ptr->wx=0;
    p_ptr->blind=p_ptr->rage=p_ptr->leaping=false;
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
    g_labyrinth_view_active=false;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        cave_set_feat(y,x,FEAT_FLOOR);
        cave_info[y][x]=CAVE_MARK|CAVE_SEEN;
        cave_color[y][x]=COLOR_STYLE_BASE;cave_light[y][x]=2;
        cave_m_idx[y][x]=cave_o_idx[y][x]=cave_when[y][x]=0;
    }
    cave_fixtures_clear();sdl_idle_animation_clear_cells();
    character_dungeon=character_generated=true;
    use_bigtile=false;Term->soft_cursor=false;
    Term->total_erase=true;prt_map();Term_fresh();
}

static void ice_transform_tests(void) {
    ice_map();
    for(int seen=0;seen<2;seen++)for(int blind=0;blind<2;blind++) {
        cave_info[10][11]=CAVE_MARK|(seen?CAVE_SEEN:0);
        p_ptr->blind=blind;
        cave_set_feat(10,11,FEAT_WATER);
        assert(project_f(-1,10,11,0,1,1,0,GF_COLD)==(seen&&!blind));
        assert(cave_feat[10][11]==FEAT_ICE);
        assert(!project_f(-1,10,11,0,1,1,0,GF_COLD));
        assert(project_f(-1,10,11,0,1,1,0,GF_FIRE)==(seen&&!blind));
        assert(cave_feat[10][11]==FEAT_WATER);
        assert(!project_f(-1,10,11,0,1,1,0,GF_FIRE));
    }
    p_ptr->blind=false;
    const int unchanged[]={FEAT_FLOOR,FEAT_WALL_EXTRA,FEAT_LAVA};
    character_dungeon=false;
    for(int i=0;i<3;i++) {
        cave_set_feat(10,11,unchanged[i]);
        assert(!project_f(-1,10,11,0,1,1,0,GF_FIRE));
        assert(!project_f(-1,10,11,0,1,1,0,GF_COLD));
        assert(cave_feat[10][11]==unchanged[i]);
    }
    character_dungeon=true;
    cave_set_feat(10,11,FEAT_WATER);
    assert(!project_f(-1,10,11,0,1,1,0,GF_POIS));
    assert(cave_feat[10][11]==FEAT_WATER);
    assert(!cave_transform_elemental_terrain(-1,11,GF_COLD));
    /* The terrain setter must invalidate standing bonuses, and melting removes scent. */
    cave_set_feat(15,15,FEAT_WATER);calc_bonuses_for_preview();p_ptr->update=0;
    int evasion=p_ptr->skill_use[S_EVN],attack=p_ptr->skill_use[S_MEL];
    assert(cave_transform_elemental_terrain(15,15,GF_COLD));
    assert(p_ptr->skill_use[S_EVN]==evasion-2&&p_ptr->skill_use[S_MEL]==attack-2);
    cave_when[15][15]=77;p_ptr->update=0;
    assert(cave_transform_elemental_terrain(15,15,GF_FIRE));
    assert(p_ptr->skill_use[S_EVN]==evasion&&p_ptr->skill_use[S_MEL]==attack);
    assert(cave_when[15][15]==0);
    object_type fire={0},cold={0};fire.k_idx=1;cold.k_idx=2;
    k_info[1].flags1=TR1_BRAND_FIRE;k_info[2].flags1=TR1_BRAND_COLD;
    cave_set_feat(10,11,FEAT_WATER);
    cave_apply_elemental_brands(10,11,&fire,&cold);
    assert(cave_feat[10][11]==FEAT_ICE);
    cave_apply_elemental_brands(10,11,&fire,&cold);
    assert(cave_feat[10][11]==FEAT_WATER);
    cave_apply_elemental_brands(10,11,NULL,NULL);assert(cave_feat[10][11]==FEAT_WATER);
    cave_apply_elemental_brands(10,11,NULL,&cold);assert(cave_feat[10][11]==FEAT_ICE);
    cave_apply_elemental_brands(10,11,&fire,NULL);assert(cave_feat[10][11]==FEAT_WATER);
    ice_map();
    for(int x=11;x<=14;x++)cave_set_feat(10,x,FEAT_WATER);
    cave_set_feat(10,13,FEAT_WALL_EXTRA);
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
    project(-1,0,10,10,10,14,1,1,0,GF_COLD,PROJECT_GRID|PROJECT_BEAM|PROJECT_HIDE,0,true);
    assert(cave_feat[10][11]==FEAT_ICE&&cave_feat[10][12]==FEAT_ICE);
    assert(cave_feat[10][13]==FEAT_WALL_EXTRA&&cave_feat[10][14]==FEAT_WATER);
    project(-1,0,10,10,10,14,1,1,0,GF_FIRE,PROJECT_GRID|PROJECT_BEAM|PROJECT_HIDE,0,true);
    assert(cave_feat[10][11]==FEAT_WATER&&cave_feat[10][12]==FEAT_WATER);
    puts("Ice transformations: production feature effects, visibility/blindness, repeat/wrong element, standing bonus refresh/scent and wall-blocked beams: PASS");
}

static void ice_expect_repaint(void) {
    Term_fresh();SDL_Surface* incremental=capture(80);
    force_map_redraw();Term_fresh();SDL_Surface* full=capture(81);
    assert(same_surface(incremental,full));
    SDL_DestroySurface(incremental);SDL_DestroySurface(full);
}

static void ice_render_tests(void) {
    ice_map();
    cave_set_feat(10,11,FEAT_ICE);cave_set_feat(10,12,FEAT_ICE);
    Term_fresh();SDL_Surface* first=capture(82);
    assert(ice_texture&&cell_count==2);
    expect_middle_frame(first,10,11,"lib/xtra/graf/ice_sheet.png");
    u64b rng=Rand_state_export();s32b turns=turn;
    int loads=image_loads,textures=texture_creations;
    for(int i=0;i<100;i++)expect_paused((1000ULL+i)*IDLE_STEP_NS);
    SDL_Surface* later=capture(83);assert(same_surface(first,later));
    assert(image_loads==loads&&texture_creations==textures);
    assert(Rand_state_export()==rng&&turn==turns);
    SDL_DestroySurface(first);SDL_DestroySurface(later);
    /* Objects and actors must survive the extra surface layer and transformations. */
    SDL_Surface* bare=capture(84);
    k_info[3].x_attr=TILE_FLAG;k_info[3].x_char=(char)(TILE_FLAG|2);
    o_list[1].k_idx=3;o_list[1].marked=true;cave_o_idx[10][11]=1;
    byte a,ta;char c,tc;map_info(10,11,&a,&c,&ta,&tc);
    assert(a==k_info[3].x_attr&&c==k_info[3].x_char);
    lite_spot(10,11);ice_expect_repaint();SDL_Surface* object=capture(85);
    assert(!same_surface(bare,object));SDL_DestroySurface(object);
    cave_transform_elemental_terrain(10,11,GF_FIRE);ice_expect_repaint();
    cave_transform_elemental_terrain(10,11,GF_COLD);ice_expect_repaint();
    cave_o_idx[10][11]=0;lite_spot(10,11);Term_fresh();
    SDL_Surface* restored=capture(86);assert(same_surface(bare,restored));
    SDL_DestroySurface(restored);
    r_info[1].x_attr=TILE_FLAG;r_info[1].x_char=(char)(TILE_FLAG|2);
    mon_list[1].r_idx=1;mon_list[1].fy=10;mon_list[1].fx=11;
    mon_list[1].ml=true;mon_list[1].alertness=ALERTNESS_ALERT;
    cave_m_idx[10][11]=1;lite_spot(10,11);ice_expect_repaint();
    SDL_Surface* actor=capture(87);assert(!same_surface(bare,actor));
    SDL_DestroySurface(actor);
    cave_transform_elemental_terrain(10,11,GF_FIRE);ice_expect_repaint();
    cave_transform_elemental_terrain(10,11,GF_COLD);ice_expect_repaint();
    cave_m_idx[10][11]=0;lite_spot(10,11);Term_fresh();
    restored=capture(88);assert(same_surface(bare,restored));
    SDL_DestroySurface(restored);SDL_DestroySurface(bare);
    for(int big=0;big<2;big++) {
        use_bigtile=big;Term->total_erase=true;
        for(int pan=0;pan<4;pan++) {
            p_ptr->wx=pan%2;p_ptr->wy=pan/2;
            prt_map();ice_expect_repaint();
        }
    }
    use_bigtile=false;p_ptr->wx=p_ptr->wy=0;Term->total_erase=true;prt_map();Term_fresh();
    for(int i=0;i<4;i++) {
        assert(cave_transform_elemental_terrain(10,11,GF_FIRE));ice_expect_repaint();
        assert(cave_transform_elemental_terrain(10,11,GF_COLD));ice_expect_repaint();
    }
    cave_set_feat(10,11,FEAT_FLOOR);ice_expect_repaint();
    cave_info[10][12]&=~CAVE_SEEN;lite_spot(10,12);ice_expect_repaint();
    assert(visible_liquid(10,12)==FEAT_ICE);
    cave_info[10][12]=0;lite_spot(10,12);ice_expect_repaint();
    assert(!visible_liquid(10,12));
    /* Produce a reviewable scene through map_info and the actual map layer renderer. */
    ice_map();
    for(int y=3;y<15;y++)for(int x=3;x<25;x++) {
        if(y==3||y==14||x==3||x==24)cave_set_feat(y,x,FEAT_WALL_EXTRA);
        else if((x>=6&&x<=11&&y>=6&&y<=11)||(x>=17&&x<=19))cave_set_feat(y,x,FEAT_ICE);
    }
    cave_set_feat(8,18,FEAT_WATER);cave_set_feat(9,18,FEAT_WATER);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,704,384);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    for(int y=3;y<15;y++)for(int x=3;x<25;x++) {
        byte a,c,ta,tc;map_info(y,x,&a,(char*)&c,&ta,(char*)&tc);
        SDL_FRect dst={(x-3)*32,(y-3)*32,32,32};
        sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
    }
    SDL_Surface* scene=SDL_RenderReadPixels(g_state.renderer,NULL);assert(scene);
    assert(IMG_SavePNG(scene,"scripts/output/ice-check/ice-scene.png"));
    SDL_DestroySurface(scene);SDL_SetRenderTarget(g_state.renderer,g_views[PANE_MAIN].canvas);
    SDL_DestroyTexture(target);
    puts("Ice rendering: exact source pixels, static scheduler, object/actor layers, pan at both tile widths, repeated melt/refreeze/removal and remembered/unseen redraw: PASS");
}

static void ice_tests(void) {
    assert(vinfo_init()==0);
    static player_race race;static character_profile profile;
    rp_ptr=&race;current_character_profile=&profile;
    c_info=calloc(1,sizeof(*c_info));p_ptr->pcharacter=0;
    cave_when=calloc(MAX_DUNGEON_HGT,sizeof(*cave_when));
    cave_natural=calloc(MAX_DUNGEON_HGT,sizeof(*cave_natural));
    mon_list=calloc(64,sizeof(*mon_list));r_info=calloc(64,sizeof(*r_info));
    o_list=calloc(64,sizeof(*o_list));k_info=calloc(64,sizeof(*k_info));
    inventory=calloc(INVEN_TOTAL,sizeof(*inventory));l_list=calloc(64,sizeof(*l_list));
    mon_max=1;
    f_info[FEAT_ICE].x_attr=TILE_FLAG;f_info[FEAT_ICE].x_char=(char)(TILE_FLAG|1);
    f_info[FEAT_WATER].x_attr=TILE_FLAG;f_info[FEAT_WATER].x_char=(char)(TILE_FLAG|1);
    ice_transform_tests();ice_render_tests();
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    idle.OUT.mkdir(parents=True, exist_ok=True)
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace(
        "    sdl_idle_animation_shutdown();\n    SDL_Quit();",
        "    ice_tests();\n    sdl_idle_animation_shutdown();\n    SDL_Quit();")
    idle.OUT = OUT
    idle.main()


if __name__ == "__main__":
    main()
