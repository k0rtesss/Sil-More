#!/usr/bin/env python3
"""Production SDL freshwater currents, depth transitions and acid checks.

Build first; isolated maps and source template parsing never open player saves.
"""
import check_idle_animation as idle
import check_cave_floor_tiles as floors
import check_terrain_transitions as shores

TESTS = r'''
#include "cave/cave-flood.h"

static const int wet_features[3]={FEAT_WATER,FEAT_DEEP_WATER,FEAT_POISON};
static SDL_Surface* chasm_fill_surface(int y,int x) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};
    draw_chasm_fill(y,x,&dst,true);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}
static void chasm_surface_tests(void) {
    f_info[FEAT_CHASM].x_attr=(byte)(TILE_FLAG|34);
    f_info[FEAT_CHASM].x_char=(char)(TILE_FLAG|0);
    assert(!load_liquid_transition_texture(FEAT_CHASM));
    for(int mask=0;mask<256;mask++) {
        shore_map(FEAT_CHASM,mask);assert(visible_liquid(10,11)==FEAT_CHASM);
        assert(liquid_transition_mask(10,11,FEAT_CHASM)==mask);
        SDL_Surface* actual=shore_surface(NULL,mask,0,true);
        SDL_Surface* expected=chasm_fill_surface(10,11);
        assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
    }
    int coords[3][2]={{-1,-1},{-1,-1},{-1,-1}};
    for(int y=0;y<32;y++)for(int x=0;x<32;x++) {
        int variant=chasm_fill_variant(y,x);
        if(coords[variant][0]<0) { coords[variant][0]=y;coords[variant][1]=x; }
    }
    SDL_Surface* variants[3];
    for(int variant=0;variant<3;variant++) {
        assert(coords[variant][0]>=0);
        variants[variant]=chasm_fill_surface(coords[variant][0],coords[variant][1]);
    }
    assert(!same_surface(variants[0],variants[1])
        && !same_surface(variants[1],variants[2])
        && !same_surface(variants[0],variants[2]));
    for(int variant=0;variant<3;variant++)SDL_DestroySurface(variants[variant]);
    shore_map(FEAT_CHASM,0);cave_feat[9][11]=FEAT_BRIDGE_CHASM_H;
    assert(liquid_transition_mask(10,11,FEAT_CHASM)==1);
    cave_info[9][11]=0;assert(liquid_transition_mask(10,11,FEAT_CHASM)==0);
    cave_info[10][11]=0;assert(!visible_liquid(10,11));
    idle_cell cell={.liquid_feat=FEAT_CHASM};assert(!cell_can_animate(&cell));
    puts("Chasms: chasm_1/2/3 fill variants, connected rendering, bridge underlay and knowledge gating: PASS");
}
static SDL_Surface* wet_expected(SDL_Texture* texture,int mask,int frame,int light,bool deep_fallback) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16); assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    bool floor_shore=liquid_is_water(cave_feat[10][11]) && !cave_water_has_icy_shore(10,11);
    if(floor_shore && mask>=0) mask=255;
    SDL_FRect dst={0,0,16,16},src={mask<0?frame*16:(mask%16)*16,
        mask<0?0:(mask/16+frame*16)*16,16,16};
    SDL_SetTextureColorMod(texture,deep_fallback?80*light/255:light,
        deep_fallback?105*light/255:light,deep_fallback?205*light/255:light);
    assert(SDL_RenderTexture(g_state.renderer,texture,&src,&dst));
    if(floor_shore) sdl_water_floor_transition_draw(10,11,&dst,NULL);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}
static void wet_atlas_tests(void) {
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];cave_water_flow_reset();
        assert(load_liquid_texture(feat));
        SDL_Texture* atlas=load_liquid_transition_texture(feat);assert(atlas);
        int count=4; /* calm pools use the four authored surface frames */
        assert(liquid_frame_count_at(10,11,feat)==count);
        for(int mask=0;mask<256;mask++) {
            shore_map(feat,mask);assert(liquid_transition_mask(10,11,feat)==mask);
            for(int frame=0;frame<count;frame++) {
                SDL_Surface* actual=shore_surface(NULL,mask,frame,true);
                SDL_Surface* expected=wet_expected(atlas,normalized_shore(mask),frame,255,false);
                assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
            }
        }
        shore_map(feat,255);
        SDL_Surface* frames[4];
        for(int frame=0;frame<count;frame++) frames[frame]=shore_surface(NULL,255,frame,true);
        assert(!same_surface(frames[0],frames[1])&&!same_surface(frames[1],frames[2]));
        assert(!same_surface(frames[2],frames[3]));
        SDL_Surface* wrapped=shore_surface(NULL,255,count,true);assert(same_surface(frames[0],wrapped));
        SDL_DestroySurface(wrapped);for(int frame=0;frame<count;frame++)SDL_DestroySurface(frames[frame]);
        cave_info[10][11]=CAVE_MARK;
        SDL_Surface* dark=shore_surface(NULL,255,2,true);
        SDL_Surface* expected=wet_expected(atlas,255,0,96,false);
        assert(same_surface(dark,expected));SDL_DestroySurface(dark);SDL_DestroySurface(expected);
    }
    assert(Rand_state_export()==rng&&turn==turns);
    puts("Freshwater/acid: all 256 shore masks, wraparound and remembered lighting without extra deep tint: PASS");
}
static SDL_Surface* wet_mixed_expected(SDL_Texture* depth,SDL_Texture* bank,int depthmask,int bankmask,int frame) {
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,16,16);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255);SDL_RenderClear(g_state.renderer);
    SDL_FRect dst={0,0,16,16};
    SDL_FRect src={(depthmask%16)*16,(depthmask/16+frame*16)*16,16,16};
    SDL_SetTextureColorMod(depth,255,255,255);assert(SDL_RenderTexture(g_state.renderer,depth,&src,&dst));
    src=(SDL_FRect){(bankmask%16)*16,(bankmask/16+frame*16)*16,16,16};
    SDL_SetTextureColorMod(bank,255,255,255);
    if(cave_water_has_icy_shore(10,11)) assert(SDL_RenderTexture(g_state.renderer,bank,&src,&dst));
    else sdl_water_floor_transition_draw(10,11,&dst,NULL);
    SDL_Surface* result=SDL_RenderReadPixels(g_state.renderer,NULL);assert(result);
    SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);return result;
}
static void wet_direction_tests(void) {
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int direction=0;direction<5;direction++)for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];SDL_Texture* atlas=load_liquid_transition_texture(feat);
        int count=direction ? 3 : 4;
        for(int mask=0;mask<256;mask++) {
            shore_map(feat,mask);cave_water_flow_set(10,11,direction);
            for(int frame=0;frame<count;frame++) {
                SDL_Surface* actual=shore_surface(NULL,mask,frame,true);
                SDL_Surface* expected=wet_expected(atlas,mask,water_page_index(direction,frame),255,false);
                assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
            }
        }
        shore_map(feat,255);cave_water_flow_set(10,11,direction);
        SDL_Surface* frames[4];
        for(int frame=0;frame<count;frame++)frames[frame]=shore_surface(NULL,255,frame,true);
        assert(!same_surface(frames[0],frames[1])&&!same_surface(frames[1],frames[2]));
        if(direction) {
            SDL_Surface* wrapped=shore_surface(NULL,255,count,true);
            assert(same_surface(frames[0],wrapped));SDL_DestroySurface(wrapped);
        } else assert(!same_surface(frames[2],frames[3]));
        for(int frame=0;frame<count;frame++)SDL_DestroySurface(frames[frame]);
        cave_info[10][11]=CAVE_MARK;
        SDL_Surface* dark=shore_surface(NULL,255,2,true);
        SDL_Surface* expected=wet_expected(atlas,255,water_page_index(direction,0),96,false);
        assert(same_surface(dark,expected));SDL_DestroySurface(dark);SDL_DestroySurface(expected);
    }
    for(int direction=1;direction<5;direction++) {
        shore_map(FEAT_WATER,0x7D);cave_water_flow_set(10,11,direction);
        cave_feat[11][11]=FEAT_DEEP_WATER;
        int depth=water_depth_transition_mask(10,11);
        for(int frame=0;frame<3;frame++) {
            SDL_Surface* actual=shore_surface(NULL,0,frame,true);
            SDL_Surface* expected=wet_mixed_expected(water_depth_transition_texture,
                water_bank_overlay_texture,depth,0x7D,water_page_index(direction,frame));
            assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        }
    }
    cave_water_flow_reset();
    assert(Rand_state_export()==rng&&turn==turns);
    puts("Freshwater: calm four-frame animation, four current directions, masks, depth/shore composition and remembered lighting: PASS");
}
static void wet_depth_tests(void) {
    shore_map(FEAT_WATER,255);cave_feat[9][11]=FEAT_DEEP_WATER;
    SDL_Surface* loaded=shore_surface(NULL,0,0,true);SDL_DestroySurface(loaded);
    assert(water_depth_transition_texture&&water_bank_overlay_texture);
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int mask=0;mask<255;mask++) {
        shore_map(FEAT_WATER,255);
        for(int i=0;i<8;i++)if(!(mask&(1<<i)))cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_DEEP_WATER;
        assert(water_depth_transition_mask(10,11)==mask);
        for(int frame=0;frame<4;frame++) {
            SDL_Surface* actual=shore_surface(NULL,mask,frame,true);
            SDL_Surface* expected=wet_mixed_expected(water_depth_transition_texture,water_bank_overlay_texture,mask,255,frame);
            assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        }
    }
    /* Shoreline and depth edges can occupy the same shallow tile, including diagonals. */
    for(int bankmask=1;bankmask<256;bankmask++) {
        shore_map(FEAT_WATER,bankmask);int deepbits=bankmask&0xAA;
        if(!deepbits)deepbits=bankmask&0x55;
        for(int i=0;i<8;i++)if(deepbits&(1<<i))cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_DEEP_WATER;
        for(int frame=0;frame<4;frame++) {
            SDL_Surface* actual=shore_surface(NULL,0,frame,true);
            SDL_Surface* expected=wet_mixed_expected(water_depth_transition_texture,water_bank_overlay_texture,255^deepbits,bankmask,frame);
            assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        }
    }
    shore_map(FEAT_WATER,255);cave_feat[9][11]=FEAT_BRIDGE_DEEP_WATER_H;
    assert(water_depth_transition_mask(10,11)==254);cave_info[9][11]=0;
    SDL_Surface* hidden=shore_surface(NULL,0,0,true);
    assert(water_depth_transition_mask(10,11)==255);
    cave_feat[9][11]=FEAT_FLOOR;SDL_Surface* absent=shore_surface(NULL,0,0,true);
    assert(same_surface(hidden,absent));SDL_DestroySurface(hidden);SDL_DestroySurface(absent);
    cave_feat[9][11]=FEAT_DEEP_WATER;cave_info[9][11]=CAVE_MARK;
    assert(water_depth_transition_mask(10,11)==254);
    p_ptr->rage=1;assert(water_depth_transition_mask(10,11)==255);p_ptr->rage=0;
    g_labyrinth_view_active=true;assert(water_depth_transition_mask(10,11)==255);g_labyrinth_view_active=false;
    assert(Rand_state_export()==rng&&turn==turns);
    shore_map(FEAT_WATER,255);cave_feat[9][11]=FEAT_DEEP_WATER;cave_info[9][11]=0;
    Term->total_erase=true;prt_map();Term_fresh();
    cave_info[9][11]=CAVE_MARK|CAVE_SEEN;lite_spot(9,11);Term_fresh();
    SDL_Surface* changed=capture(130);force_map_redraw();Term_fresh();SDL_Surface* full=capture(131);
    assert(same_surface(changed,full));SDL_DestroySurface(changed);SDL_DestroySurface(full);
    cave_set_feat(9,11,FEAT_WATER);Term_fresh();changed=capture(132);
    force_map_redraw();Term_fresh();full=capture(133);assert(same_surface(changed,full));
    SDL_DestroySurface(changed);SDL_DestroySurface(full);
    puts("Freshwater depth composition: all depth masks and mixed land masks, diagonals/bridges, hidden-depth gating and discovery/removal repaint: PASS");
}
static void wet_knowledge_tests(void) {
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];shore_map(feat,255);
        if(feat!=FEAT_POISON) {
            for(int i=0;i<8;i++) cave_feat[10+shore_dy[i]][11+shore_dx[i]]=
                i%2?FEAT_WATER:FEAT_DEEP_WATER;
            assert(liquid_transition_mask(10,11,feat)==255);
            cave_feat[9][11]=FEAT_BRIDGE_DEEP_WATER_H;
            assert(liquid_transition_mask(10,11,feat)==255);
        } else {
            cave_feat[9][11]=FEAT_WATER;assert(liquid_transition_mask(10,11,feat)==254);
            cave_feat[9][11]=FEAT_BRIDGE_POISON_H;assert(liquid_transition_mask(10,11,feat)==255);
        }
        for(int i=0;i<8;i++) cave_info[10+shore_dy[i]][11+shore_dx[i]]=0;
        assert(liquid_transition_mask(10,11,feat)==0);
        SDL_Surface* hidden=shore_surface(NULL,0,0,true);
        for(int i=0;i<8;i++) cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_FLOOR;
        SDL_Surface* absent=shore_surface(NULL,0,0,true);assert(same_surface(hidden,absent));
        SDL_DestroySurface(hidden);SDL_DestroySurface(absent);
        shore_map(feat,255);
        for(int i=0;i<8;i++) cave_info[10+shore_dy[i]][11+shore_dx[i]]=CAVE_MARK;
        p_ptr->rage=1;assert(liquid_transition_mask(10,11,feat)==0);p_ptr->rage=0;
        g_labyrinth_view_active=true;assert(liquid_transition_mask(10,11,feat)==0);
        g_labyrinth_view_active=false;
    }
    puts("Shallow/deep share connectivity through bridges; acid separate; unknown pixels and rage/labyrinth hide knowledge: PASS");
}
static void wet_bank_tests(void) {
    u64b rng=Rand_state_export();s32b turns=turn;
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind],inner_seen=0;
        int first=feat==FEAT_POISON?16:0;
        for(int y=4;y<24;y+=2)for(int x=4;x<24;x+=2) {
            floor_reset(13);int normal=floor_tile(y,x+3);
            cave_feat[y][x]=feat;
            int inner=floor_tile(y,x+1),outer=floor_tile(y,x+2);
            if(feat==FEAT_POISON) {
                assert(inner>>8==36&&outer==normal);
                assert((inner&255)>=first&&(inner&255)<=first+6&&!(inner&1));
                inner_seen|=1<<(((inner&255)-first)/2);
            } else assert(inner==normal && outer==normal);
            assert(floor_tile(y,x+3)==normal);
            for(int n=0;n<5;n++)assert(floor_tile(y,x+1)==inner&&floor_tile(y,x+2)==outer);
            cave_info[y][x]=0;assert(floor_tile(y,x+1)==normal&&floor_tile(y,x+2)==normal);
            cave_info[y][x]=CAVE_MARK;p_ptr->rage=1;
            assert(floor_tile(y,x+1)==normal&&floor_tile(y,x+2)==normal);p_ptr->rage=0;
            g_labyrinth_view_active=true;
            assert(floor_tile(y,x+1)==normal&&floor_tile(y,x+2)==normal);g_labyrinth_view_active=false;
        }
        if(feat==FEAT_POISON)assert(inner_seen==15);
    }
    floor_reset(13); cave_flood_clear();
    p_ptr->energy_use=100; p_ptr->leaving=false; p_ptr->is_dead=false;
    int normal_one=floor_tile(10,9), normal_two=floor_tile(10,8),
        normal_three=floor_tile(10,7);
    cave_set_feat(10,10,FEAT_WATER);
    assert(cave_flood_restore(10,10,1));
    assert(cave_flood_surface_at(10,10));
    assert(floor_tile(10,9)==normal_one);
    cave_flood_begin_action(); cave_flood_end_action();
    assert(cave_feat[10][9]==FEAT_WATER
        && cave_flood_surface_at(10,9));
    assert(floor_tile(10,8)==normal_two);
    cave_flood_begin_action(); cave_flood_end_action();
    assert(cave_feat[10][10]==FEAT_DEEP_WATER
        && cave_flood_stage_at(10,10)==CAVE_FLOOD_STAGE_COMPLETE);
    assert(floor_tile(10,7)==normal_three);
    floor_reset(13); cave_flood_clear();
    int acid_normal_one=floor_tile(10,9), acid_normal_two=floor_tile(10,8),
        acid_normal_three=floor_tile(10,7);
    cave_set_feat(10,10,FEAT_POISON);
    assert(cave_flood_restore(10,10,1));
    assert(cave_flood_surface_at(10,10));
    assert(floor_tile(10,9)==acid_normal_one);
    cave_flood_begin_action(); cave_flood_end_action();
    assert(cave_feat[10][9]==FEAT_POISON
        && cave_flood_surface_at(10,9));
    assert(floor_tile(10,8)==acid_normal_two);
    cave_flood_begin_action(); cave_flood_end_action();
    assert(cave_feat[10][10]==FEAT_POISON
        && cave_flood_stage_at(10,10)==CAVE_FLOOD_STAGE_COMPLETE);
    assert(floor_tile(10,7)==acid_normal_three);
    assert(Rand_state_export()==rng&&turn==turns);
    puts("Water retains the original floor; natural acid retains its bank; acid floods stay bankless: PASS");
}
static void wet_redraw_tests(void) {
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];shore_map(feat,1);cave_info[9][11]=0;
        Term->total_erase=true;frame_tick=0;prt_map();Term_fresh();
        byte a,ta;char c,tc;map_info(10,11,&a,&c,&ta,&tc);
        SDL_Surface* before=capture(110);
        cave_info[9][11]=CAVE_MARK|CAVE_SEEN;lite_spot(9,11);Term_fresh();
        byte a2,ta2;char c2,tc2;map_info(10,11,&a2,&c2,&ta2,&tc2);
        assert(a==a2&&c==c2&&ta==ta2&&tc==tc2);
        SDL_Surface* changed=capture(111);assert(!same_surface(before,changed));
        force_map_redraw();Term_fresh();SDL_Surface* full=capture(112);assert(same_surface(changed,full));
        SDL_DestroySurface(before);SDL_DestroySurface(changed);SDL_DestroySurface(full);
        cave_set_feat(9,11,FEAT_FLOOR);Term_fresh();changed=capture(113);
        force_map_redraw();Term_fresh();full=capture(114);assert(same_surface(changed,full));
        SDL_DestroySurface(changed);SDL_DestroySurface(full);
        /* A lone source exercises bank invalidation without a second source masking it. */
        floor_reset(13);cave_feat[10][11]=feat;cave_info[10][11]=0;
        Term->total_erase=true;prt_map();Term_fresh();
        cave_info[10][11]=CAVE_MARK|CAVE_SEEN;lite_spot(10,11);Term_fresh();changed=capture(115);
        force_map_redraw();Term_fresh();full=capture(116);assert(same_surface(changed,full));
        SDL_DestroySurface(changed);SDL_DestroySurface(full);
        cave_set_feat(10,11,FEAT_FLOOR);Term_fresh();changed=capture(117);
        force_map_redraw();Term_fresh();full=capture(118);assert(same_surface(changed,full));
        SDL_DestroySurface(changed);SDL_DestroySurface(full);
    }
    puts("Same-glyph liquid connectivity and single bank reveal/remove repaint identically to full map: PASS");
}
static void wet_layer_tests(void) {
    free(r_info);r_info=calloc(8,sizeof(*r_info));mon_list=calloc(8,sizeof(*mon_list));
    o_list=calloc(8,sizeof(*o_list));k_info=calloc(8,sizeof(*k_info));mon_max=1;
    for(int kind=0;kind<3;kind++) {
        shore_map(wet_features[kind],255);
        if(kind<3)cave_water_flow_set(10,11,CAVE_WATER_FLOW_SOUTH);
        frame_tick=0;Term->total_erase=true;prt_map();Term_fresh();
        SDL_Surface* bare=capture(120);
        k_info[3].x_attr=TILE_FLAG;k_info[3].x_char=(char)(TILE_FLAG|2);
        o_list[1].k_idx=3;o_list[1].marked=true;cave_o_idx[10][11]=1;
        lite_spot(10,11);Term_fresh();SDL_Surface* object=capture(121);assert(!same_surface(bare,object));
        SDL_DestroySurface(object);cave_o_idx[10][11]=0;
        r_info[1].x_attr=TILE_FLAG;r_info[1].x_char=(char)(TILE_FLAG|2);
        mon_list[1].r_idx=1;mon_list[1].fy=10;mon_list[1].fx=11;
        mon_list[1].ml=true;mon_list[1].alertness=ALERTNESS_ALERT;cave_m_idx[10][11]=1;
        lite_spot(10,11);Term_fresh();SDL_Surface* actor=capture(122);assert(!same_surface(bare,actor));
        SDL_DestroySurface(actor);cave_m_idx[10][11]=0;lite_spot(10,11);Term_fresh();
        SDL_Surface* restored=capture(123);assert(same_surface(bare,restored));
        SDL_DestroySurface(restored);
        int bridge=kind==0?FEAT_BRIDGE_WATER_H:kind==1?FEAT_BRIDGE_DEEP_WATER_H:FEAT_BRIDGE_POISON_H;
        f_info[bridge].x_char=(char)(TILE_FLAG|1);cave_feat[10][11]=bridge;
        lite_spot(10,11);Term_fresh();SDL_Surface* deck=capture(124);
        assert(!same_surface(bare,deck));force_map_redraw();Term_fresh();
        restored=capture(125);assert(same_surface(deck,restored));
        SDL_DestroySurface(deck);SDL_DestroySurface(restored);SDL_DestroySurface(bare);
        int seen_frames=0;Uint64 animated_now=frame_tick*IDLE_STEP_NS;
        for(int step=0;step<32;step++) {
            animated_now+=IDLE_STEP_NS;sdl_idle_animation_update(animated_now);
            for(int i=0;i<cell_count;i++)if(cells[i].y==10&&cells[i].x==11) {
                assert(cells[i].drawn_frame<3);seen_frames|=1<<cells[i].drawn_frame;
            }
        }
        assert(seen_frames==7);
    }
    int loads=image_loads,textures=texture_creations,mallocs=allocations;
    u64b rng=Rand_state_export();s32b turns=turn;Uint64 now=frame_tick*IDLE_STEP_NS;
    for(int i=0;i<2000;i++){now+=IDLE_STEP_NS;sdl_idle_animation_update(now);}
    assert(loads==image_loads&&textures==texture_creations&&mallocs==allocations);
    assert(Rand_state_export()==rng&&turn==turns);
    puts("Objects/actors remain above all new surfaces; cached idle frames make no loads/textures/allocations/RNG changes: PASS");
}
static void wet_fallback_tests(void) {
    sdl_idle_animation_shutdown();missing_transition=true;
    for(int kind=0;kind<3;kind++) {
        int feat=wet_features[kind];shore_map(feat,0);assert(load_liquid_texture(feat));
        SDL_Surface* actual=shore_surface(NULL,0,0,true);
        SDL_Surface* expected=wet_expected(feat==FEAT_POISON?poison_texture:water_texture,-1,0,255,feat==FEAT_DEEP_WATER);
        assert(same_surface(actual,expected));SDL_DestroySurface(actual);SDL_DestroySurface(expected);
        assert(!load_liquid_transition_texture(feat));int loads=image_loads;
        for(int n=0;n<5;n++){actual=shore_surface(NULL,0,n%3,true);SDL_DestroySurface(actual);}
        assert(image_loads==loads);
    }
    missing_transition=false;sdl_idle_animation_shutdown();
    puts("Missing river/deep/acid transition atlases retain original fallback and cache failure: PASS");
}
static void wet_preview(void) {
    const int w=26,h=19,scale=2;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*2,h*16*scale);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int panel=0;panel<2;panel++) {
        floor_reset(13);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            bool wall=y==0||y==h-1||x==0||x==w-1;
            bool pool=y>=8&&y<=15&&x>=7&&x<=21&&!(y<=9&&x<=8)&&!(y>=14&&x>=19);
            bool river=(y>=1&&y<=5&&x>=3&&x<=4)
                ||(y>=4&&y<=5&&x>=4&&x<=12)
                ||(y>=5&&y<=10&&x>=11&&x<=12);
            bool shoal=y<=10||y>=14||x<=9||x>=20||(x>=13&&x<=15&&y<=12);
            int wet=panel?FEAT_POISON:shoal?FEAT_WATER:FEAT_DEEP_WATER;
            cave_feat[y][x]=wall?FEAT_WALL_EXTRA:(pool||river)?wet:FEAT_FLOOR;
            cave_info[y][x]=CAVE_MARK|CAVE_SEEN|CAVE_GLOW|(wall?CAVE_WALL:0);
            cave_water_flow_set(y,x,!panel&&river&&!pool?(y>=4&&x<11?2:3):0);
        }
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            byte a,ta;char c,tc;map_info(y,x,&a,&c,&ta,&tc);
            SDL_FRect dst={(panel*w+x)*16*scale,y*16*scale,16*scale,16*scale};
            sdl_draw_map_tile_layers_at(y,x,a,c,ta,tc,&dst);
        }
    }
    for(int i=0;i<3;i++)assert(load_liquid_transition_texture(wet_features[i]));
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/river-acid-tiles-check/preview.png"));
    SDL_DestroySurface(surface);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    cave_water_flow_reset();
}
static void floor_shore_tests(void) {
    /* Walls/unknown terrain must contribute no pixels, and changing the
     * adjacent floor must change the edge without changing its wet centre. */
    for(int kind=0;kind<2;kind++) {
        int feat=wet_features[kind];shore_map(feat,0);cave_water_flow_reset();
        for(int i=0;i<8;i++)cave_feat[10+shore_dy[i]][11+shore_dx[i]]=FEAT_WALL_EXTRA;
        SDL_Surface* wall=shore_surface(NULL,0,0,true);
        for(int i=0;i<8;i++)cave_info[10+shore_dy[i]][11+shore_dx[i]]=0;
        SDL_Surface* hidden=shore_surface(NULL,0,0,true);
        assert(same_surface(wall,hidden));SDL_DestroySurface(hidden);
        cave_feat[10][10]=FEAT_FLOOR;cave_info[10][10]=CAVE_MARK|CAVE_SEEN;
        cave_color[10][10]=COLOR_STYLE_BASE+13;
        SDL_Surface* floor=shore_surface(NULL,0,0,true);
        assert(!same_surface(wall,floor));
        cave_color[10][10]=COLOR_STYLE_BASE+44;
        SDL_Surface* changed=shore_surface(NULL,0,0,true);
        assert(!same_surface(floor,changed));
        for(int py=0;py<16;py++)for(int px=5;px<16;px++) {
            Uint8 r,g,b,a,rr,gg,bb,aa;
            assert(SDL_ReadSurfacePixel(wall,px,py,&r,&g,&b,&a));
            assert(SDL_ReadSurfacePixel(changed,px,py,&rr,&gg,&bb,&aa));
            assert(r==rr&&g==gg&&b==bb&&a==aa);
        }
        /* Cropped liquid donors (e.g. beside a chasm) match the full cell. */
        SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
        SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,16,16);assert(target);SDL_SetRenderTarget(g_state.renderer,target);
        for(int y=0;y<16;y+=4)for(int x=0;x<16;x+=4) {
            SDL_FRect piece={x,y,4,4};assert(draw_liquid_region(10,11,&piece,&piece));
        }
        SDL_Surface* pieces=SDL_RenderReadPixels(g_state.renderer,NULL);assert(pieces);
        assert(same_surface(changed,pieces));SDL_DestroySurface(pieces);
        SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
        SDL_DestroySurface(wall);SDL_DestroySurface(floor);SDL_DestroySurface(changed);
    }
    puts("Water edges use actual floor styles; walls/unknown cells add no rim; cropped donors match whole tiles: PASS");
}
static void flood_preview(void) {
    const int w=13,h=11,scale=3;
    SDL_Texture* previous=SDL_GetRenderTarget(g_state.renderer);
    SDL_Texture* target=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,w*16*scale*3,h*16*scale*2);assert(target);
    SDL_SetRenderTarget(g_state.renderer,target);
    for(int scene=0;scene<2;scene++) {
        floor_reset(13);cave_flood_clear();cave_water_flow_reset();
        p_ptr->energy_use=100;p_ptr->leaving=false;p_ptr->is_dead=false;
        p_ptr->py=p_ptr->px=28;
        for(int y=1;y<32;y++)for(int x=1;x<32;x++) {
            bool wall=y==2||y==h+1||x==2||x==w+1;
            if(scene && ((x==7&&y>=3&&y<=6)||(x==10&&y>=7&&y<=9)))wall=true;
            cave_set_feat(y,x,wall?FEAT_WALL_EXTRA:FEAT_FLOOR);
            cave_info[y][x]|=CAVE_MARK|CAVE_SEEN;
        }
        if(scene) {
            cave_set_feat(7,7,FEAT_RUBBLE);cave_set_feat(8,7,FEAT_DOOR_HEAD);
            cave_set_feat(8,8,FEAT_BRIDGE_WATER_H);
        }
        cave_set_feat(7,6,FEAT_WATER);assert(cave_flood_restore(7,6,1));
        for(int stage=0;stage<3;stage++) {
            if(stage){cave_flood_begin_action();cave_flood_end_action();}
            for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
                byte a,ta;char c,tc;map_info(y+2,x+2,&a,&c,&ta,&tc);
                SDL_FRect dst={(stage*w+x)*16*scale,(scene*h+y)*16*scale,16*scale,16*scale};
                sdl_draw_map_tile_layers_at(y+2,x+2,a,c,ta,tc,&dst);
            }
        }
    }
    SDL_Surface* surface=SDL_RenderReadPixels(g_state.renderer,NULL);assert(surface);
    assert(IMG_SavePNG(surface,"scripts/output/river-acid-tiles-check/flood-turns.png"));
    SDL_DestroySurface(surface);SDL_SetRenderTarget(g_state.renderer,previous);SDL_DestroyTexture(target);
    cave_flood_clear();
}
'''


def main():
    idle.OUT = idle.ROOT / "scripts/output/river-acid-tiles-check"
    idle.HARNESS = idle.HARNESS.replace("scripts/output/idle-animation-check", "scripts/output/river-acid-tiles-check")
    idle.HARNESS = idle.HARNESS.replace("static bool missing_asset;", "static bool missing_asset, missing_transition;")
    idle.HARNESS = idle.HARNESS.replace("return missing_asset ? NULL : IMG_Load(path);",
        'return missing_asset || (missing_transition && strstr(path,"transition_")) ? NULL : IMG_Load(path);')
    idle.HARNESS = idle.HARNESS.replace("int main(void) {", floors.TESTS + shores.TESTS + TESTS + "\nint main(void) {")
    idle.HARNESS = idle.HARNESS.replace("    asynchronous_tests();",
        "    asynchronous_tests();\n    floor_templates();\n"
        "    f_info[FEAT_WATER].x_char=f_info[FEAT_DEEP_WATER].x_char=f_info[FEAT_POISON].x_char=(char)(TILE_FLAG|1);\n"
        "    chasm_surface_tests();\n    wet_atlas_tests();\n    wet_depth_tests();\n    wet_knowledge_tests();\n    wet_bank_tests();\n    wet_redraw_tests();\n"
        "    wet_direction_tests();\n    wet_layer_tests();\n    wet_fallback_tests();\n    wet_preview();\n    floor_shore_tests();\n    flood_preview();")
    idle.main()


if __name__ == "__main__":
    main()
