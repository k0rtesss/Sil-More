/* Production sources are included so transactional candidate invariants can
 * be exercised directly, without maintaining a second generator in tests. */
#include "level-generation/level-generation-terrain.c"
#include <assert.h>
#include <stdio.h>
bool test_themes_parse(const char* text, size_t size);

/* These fixtures isolate local accents and deliberately retain partition-wall
 * invariants. The separately linked landmark suite exercises level geology. */
void terrain_landmark_reset(void) {}
bool place_terrain_landmark(void) { return false; }
bool terrain_history_active(void) { return false; }
bool terrain_history_started(void) { return false; }
int terrain_history_count(void) { return 0; }
const terrain_landmark_stats* terrain_landmark_system_stats(int system) {
    (void)system; return terrain_landmark_last_stats();
}
const terrain_landmark_stats* terrain_landmark_last_stats(void) {
    static const terrain_landmark_stats disabled = {0}; return &disabled;
}
bool terrain_landmark_partition(int partition) { (void)partition; return false; }

static player_type player;
player_type* p_ptr = &player;
static dun_data dungeon;
dun_data* dun = &dungeon;
static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte original[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte natural[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID] = features;
byte (*cave_natural)[MAX_DUNGEON_WID] = natural;
static u16b info[MAX_DUNGEON_HGT][256];
u16b (*cave_info)[256] = info;
static s16b monsters[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b objects[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
s16b (*cave_m_idx)[MAX_DUNGEON_WID] = monsters;
s16b (*cave_o_idx)[MAX_DUNGEON_WID] = objects;
static object_type test_objects[2];
object_type* o_list = test_objects;
s16b o_max = 2;
layout_anchor_kind_t room_anchor_kind[CENT_MAX];
quadrant_mode_t current_partition_modes[25];
big_cave_type_t current_partition_big_cave_types[25];
int current_partition_rows=1, current_partition_cols=1, current_partition_count=1;
static unsigned state;
static unsigned running_seed;
static void dump_map(const char* name);
u32b Rand_div(u32b n) { state=state*1664525u+1013904223u; return n ? (state>>8)%n : 0; }
int distance(int y1,int x1,int y2,int x2) {
    int y=ABS(y1-y2),x=ABS(x1-x2);return MAX(y,x)+MIN(y,x)/2;
}
int level_partition_index_for_point(int y,int x) {
    (void)y;return current_partition_count==1?0:(x<40?0:1);
}
byte cave_fixture_at(int y,int x) {return y==12&&x==13;}
bool coord_in_morgoth_region(int y,int x,int margin) {(void)y;(void)x;(void)margin;return false;}
bool generation_escape_tunnel_bold(int y,int x) {(void)y;(void)x;return false;}
void cave_set_feat(int y,int x,int feat) {features[y][x]=feat;}
void log_log(int level,const char* file,int line,const char* fmt,...) {
    (void)level;(void)file;(void)line;(void)fmt;
}

static const int material_features[]={FEAT_WATER,FEAT_CHASM,FEAT_LAVA,FEAT_POISON,FEAT_ICE};
static const char* material_names[]={"water","chasm","lava","poison","ice"};

static void select_material(int material, int pool_chance) {
    /* Use the production parser and a complete, valid single-material file. */
    char text[4096];int length=snprintf(text,sizeof(text),"V:1\n");
    for(int depth=1;depth<=20;depth++) {
        int chosen=material<0?depth%5:material;
        length+=snprintf(text+length,sizeof(text)-length,
        "D:%d:Test:%d:%d:%d:%d:%d:100:12:24:3:%d\n",depth,
        chosen==0,chosen==1,chosen==2,chosen==3,chosen==4,pool_chance);
    }
    assert(test_themes_parse(text,length));
}

static void reset_map(int shape) {
    memset(&player,0,sizeof(player));memset(&dungeon,0,sizeof(dungeon));
    memset(features,FEAT_WALL_EXTRA,sizeof(features));memset(info,0,sizeof(info));
    memset(monsters,0,sizeof(monsters));memset(objects,0,sizeof(objects));
    memset(natural,0,sizeof(natural));memset(room_anchor_kind,0,sizeof(room_anchor_kind));
    memset(current_partition_modes,0,sizeof(current_partition_modes));
    memset(current_partition_big_cave_types,0,sizeof(current_partition_big_cave_types));
    current_partition_count=current_partition_rows=current_partition_cols=1;
    player.cur_map_hgt=40;player.cur_map_wid=80;player.depth=10;
    current_partition_modes[0]=shape?QUAD_MODE_LABYRINTH:QUAD_MODE_CAVEY;
    for(int y=4;y<36;y++)for(int x=4;x<76;x++) {
        /* Maze fixture has alternating thin walls and open chamber ends. */
        if(shape==1 && x>17&&x<63 && x%6==0 && y>7&&y<32 && y!=(x%12?12:27)) continue;
        if(shape==2 && x>=37&&x<=41 && y!=20) continue;
        features[y][x]=FEAT_FLOOR;info[y][x]=CAVE_ROOM;natural[y][x]=1;
    }
    dun->cent_n=2;dun->cent[0]=(coord){20,8};dun->cent[1]=(coord){20,71};
    dun->corner[0]=(rectangle){4,4,35,37};dun->corner[1]=(rectangle){4,42,35,75};
    room_anchor_kind[0]=room_anchor_kind[1]=LAYOUT_ANCHOR_CA_BLOB;
    features[10][10]=FEAT_MORE;features[30][70]=FEAT_LESS;
    features[11][10]=FEAT_FORGE_NORMAL_HEAD;features[12][10]=FEAT_DOOR_HEAD;
    objects[11][11]=1;monsters[11][12]=1;
    test_objects[1].name1=1; /* Authored valuables must never be flooded. */
    info[15][15]|=CAVE_ICKY;info[15][16]|=CAVE_G_VAULT;
    memcpy(original,features,sizeof(original));terrain_generation_reset();
}

static int check_map(int feature) {
    static byte reached[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    terrain_generation_flood(10,10,reached,NULL);
    int count=0;
    for(int y=1;y<39;y++)for(int x=1;x<79;x++) {
        if((original[y][x]!=FEAT_FLOOR && (original[y][x]!=FEAT_WALL_EXTRA
            || current_partition_modes[level_partition_index_for_point(y,x)]==QUAD_MODE_LABYRINTH))
            ||objects[y][x]||monsters[y][x]
            ||cave_fixture_at(y,x)||coord_in_morgoth_region(y,x,0)||generation_escape_tunnel_bold(y,x)
            ||(info[y][x]&(CAVE_ICKY|CAVE_G_VAULT|CAVE_MORGOTH_TUNNEL)))
            assert(features[y][x]==original[y][x]);
        if(terrain_generation_walkable(y,x,NULL)) assert(reached[y][x]);
        if(features[y][x]!=feature) continue;
        count++;
        assert(original[y][x]==FEAT_FLOOR || (original[y][x]==FEAT_WALL_EXTRA
            && current_partition_modes[level_partition_index_for_point(y,x)]==QUAD_MODE_CAVEY));
        /* Every generated cell belongs to a real four-connected shape. */
        bool adjacent=features[y-1][x]==feature||features[y+1][x]==feature
            ||features[y][x-1]==feature||features[y][x+1]==feature;
        if(!adjacent) {
            fprintf(stderr,"Singleton seed=%u feature=%d mode=%d y=%d x=%d\n",running_seed,feature,current_partition_modes[0],y,x);
            dump_map("failure-singleton");
        }
        assert(adjacent);
    }
    assert(features[20][8]==FEAT_FLOOR&&features[20][71]==FEAT_FLOOR);
    return count;
}

static void dump_map(const char* name) {
    char path[256];snprintf(path,sizeof(path),"scripts/output/terrain-generation-check/map-%s.txt",name);
    FILE* out=fopen(path,"w");assert(out);
    for(int y=0;y<40;y++) {
        for(int x=0;x<80;x++) {
            char c='#';int f=features[y][x];
            if(f==FEAT_FLOOR)c=terrain_generation_reserved(y,x)?'r':'.';
            for(int m=0;m<5;m++)if(f==material_features[m])c="~CLPI"[m];
            if(f==FEAT_DEEP_WATER)c='D';
            if(FEAT_IS_BRIDGE(f))c='B';
            if(original[y][x]!=FEAT_FLOOR&&original[y][x]!=FEAT_WALL_EXTRA)c='!';
            if(objects[y][x]||monsters[y][x]||cave_fixture_at(y,x))c='!';
            fputc(c,out);
        }fputc('\n',out);
    }fclose(out);
}

static void barrier(terrain_candidate* c,int x,int feature) {
    memset(c,0,sizeof(*c));c->feature=feature;c->partition=0;
    for(int y=4;y<36;y++)c->path[c->path_count++]=(coord){y,x};
    terrain_shape(c,1,false);
}

static void candidate_tests(void) {
    terrain_candidate c;
    /* Deep patches preserve a shallow shoreline and bridge material/axis. */
    reset_map(0);
    for(int y=17;y<=23;y++)for(int x=30;x<=40;x++)features[y][x]=FEAT_WATER;
    for(int x=30;x<=40;x++)features[20][x]=FEAT_BRIDGE_WATER_H;
    terrain_deepen_water();
    assert(features[17][35]==FEAT_WATER);
    int deep_count=0, core_count=0;
    for(int y=18;y<=22;y++)for(int x=31;x<=39;x++) {
        core_count++;deep_count+=features[y][x]==FEAT_DEEP_WATER;
    }
    assert(deep_count>0&&deep_count<core_count);
    for(int x=30;x<=40;x++)
        if(cave_bridge_underlay(features[20][x])==FEAT_DEEP_WATER)
            assert(features[20][x]==FEAT_BRIDGE_DEEP_WATER_H);
    assert(terrain_generation_walkable(18,35,NULL));
    assert(terrain_generation_walkable(20,35,NULL));
    assert(!terrain_generation_jump(17,35,1,0,NULL));
    assert(cave_bridge_feature(FEAT_DEEP_WATER,true)==FEAT_BRIDGE_DEEP_WATER_V);
    /* Narrow channels retain shallow wading access. */
    reset_map(0);
    for(int y=18;y<=19;y++)for(int x=30;x<=40;x++)features[y][x]=FEAT_WATER;
    terrain_deepen_water();
    assert(features[18][35]==FEAT_WATER&&features[19][35]==FEAT_WATER);

    reset_map(0);terrain_context();terrain_components(features,baseline);
    barrier(&c,40,FEAT_CHASM);
    assert(!memcmp(features,original,sizeof(features)));
    assert(terrain_preserves_access());
    terrain_commit(&c,TERRAIN_THEME_CHASM);
    /* The wall-to-wall fracture really splits dry land; it is accepted because
     * its complete candidate retains bidirectional jump routes, without a bridge. */
    for(int y=4;y<36;y++)assert(features[y][40]==FEAT_CHASM);
    assert(terrain_generation_jump(20,39,0,1,NULL));
    assert(terrain_generation_jump(20,41,0,-1,NULL));
    assert(terrain_generation_reserved(20,39)&&terrain_generation_reserved(20,41));
    place_rubble(20,39);place_rubble(20,41);
    assert(features[20][39]==FEAT_FLOOR&&features[20][41]==FEAT_FLOOR);
    assert(terrain_generation_last_stats()->bridges==0);
    dump_map("whole-candidate-jump-fracture");

    /* Multiple separated hazards still form a valid whole-map route. */
    barrier(&c,50,FEAT_POISON);assert(terrain_preserves_access());
    terrain_commit(&c,TERRAIN_THEME_POISON);check_map(FEAT_CHASM);check_map(FEAT_POISON);
    dump_map("multiple-hazards");

    reset_map(0);terrain_context();terrain_components(features,baseline);
    barrier(&c,40,FEAT_LAVA);
    for(int y=4;y<36;y++)assert(terrain_add_cell(&c,y,41,false));
    assert(!terrain_preserves_access());
    assert(!memcmp(features,original,sizeof(features))); /* Rejection is atomic. */
    assert(terrain_generation_last_stats()->accepted==0);
    for(int y=0;y<40;y++)for(int x=0;x<80;x++)assert(!terrain_generation_reserved(y,x));

    /* An isolated widening spur is invalid even if traversal remains fine. */
    reset_map(0);terrain_context();terrain_components(features,baseline);
    barrier(&c,40,FEAT_WATER);assert(terrain_add_cell(&c,20,45,false));
    assert(terrain_preserves_access()&&!terrain_candidate_valid(&c));
    assert(!memcmp(features,original,sizeof(features)));

    /* A one-square landing in a straight one-square corridor has no runup;
     * open strips can legitimately use diagonal leaps and sideways runups. */
    reset_map(0);memset(features,FEAT_WALL_EXTRA,sizeof(features));
    for(int x=4;x<76;x++)features[20][x]=FEAT_FLOOR;
    memcpy(original,features,sizeof(original));terrain_context();terrain_components(features,baseline);
    barrier(&c,40,FEAT_CHASM);
    assert(terrain_add_cell(&c,20,42,false));
    assert(!terrain_preserves_access());assert(!memcmp(features,original,sizeof(features)));

    /* Production endpoint selection, routing, width growth and outlet basins
     * must produce a single connected proposal anchored to real boundaries. */
    int shapes=0;select_material(TERRAIN_THEME_WATER,100);
    for(unsigned seed=1;seed<=100;seed++) {
        reset_map(seed%2);state=seed;running_seed=seed;terrain_context();coord start,end;bool pool;
        assert(terrain_endpoints(0,terrain_theme_for_depth(10),&start,&end,&pool));
        memset(&c,0,sizeof(c));c.feature=FEAT_WATER;c.endpoint_pool=pool;
        c.path_count=terrain_route(start,end,0,FEAT_WATER,24,c.path);
        if(!c.path_count)continue;
        assert(terrain_mouth(start.y,start.x,0));
        if(!pool)assert(terrain_mouth(end.y,end.x,0));
        else {
            int floors=0;
            for(int d=0;d<4;d++)floors+=features[end.y+channel_dy[d]][end.x+channel_dx[d]]==FEAT_FLOOR;
            assert(floors>=3);
        }
        for(int i=1;i<c.path_count;i++)assert(ABS(c.path[i].y-c.path[i-1].y)+ABS(c.path[i].x-c.path[i-1].x)==1);
        terrain_shape(&c,3,true);
        static byte seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
        memset(seen,0,sizeof(seen));coord queue[TERRAIN_SHAPE_MAX];int head=0,tail=0;
        queue[tail++]=c.cells[0];seen[c.cells[0].y][c.cells[0].x]=1;
        while(head<tail) {
            coord p=queue[head++];
            for(int d=0;d<4;d++) {
                int y=p.y+channel_dy[d],x=p.x+channel_dx[d];
                if(!proposed[y][x]||seen[y][x])continue;
                seen[y][x]=1;queue[tail++]=(coord){y,x};
            }
        }
        assert(tail==c.count);shapes++;
    }
    assert(shapes>50);
    /* Parser-valid fixed lengths must have a nonempty endpoint domain. */
    terrain_theme_profile fixed=*terrain_theme_for_depth(10);
    fixed.min_length=fixed.max_length=6;fixed.max_width=1;fixed.pool_chance=0;
    int fixed_count=0;
    for(unsigned seed=1;seed<=40;seed++) {
        reset_map(0);state=seed;running_seed=seed;terrain_context();terrain_components(features,baseline);
        fixed_count+=terrain_region(0,TERRAIN_THEME_WATER,&fixed);
    }
    assert(fixed_count>0);

    /* Architectural crossings protect all of their checked approach cells. */
    reset_map(0);current_partition_modes[0]=QUAD_MODE_LABYRINTH;
    terrain_context();terrain_components(features,baseline);barrier(&c,40,FEAT_LAVA);
    assert(terrain_architectural_bridge(&c));assert(c.bridge_approach_count>=5);
    assert(terrain_preserves_access());terrain_commit(&c,TERRAIN_THEME_LAVA);
    for(int i=0;i<c.bridge_approach_count;i++) {
        coord p=c.bridge_approaches[i];
        assert(features[p.y][p.x]==FEAT_FLOOR&&terrain_generation_reserved(p.y,p.x));
        place_rubble(p.y,p.x);assert(features[p.y][p.x]==FEAT_FLOOR);
    }
    place_rubble(34,70);assert(features[34][70]==FEAT_RUBBLE);
    dump_map("architectural-crossing");
    puts("Whole candidates: jump-only fracture, multiple hazards, atomic rollback, no jump chains, boundary endpoints and connected expansion: PASS");
}

static void deep_water_tests(void) {
    reset_map(0);
    for(int y=14;y<=24;y++)for(int x=14;x<=30;x++)features[y][x]=FEAT_WATER;
    for(int x=14;x<=30;x++)features[19][x]=FEAT_BRIDGE_WATER_H;
    terrain_deepen_water();
    assert(features[14][20]==FEAT_WATER&&features[20][14]==FEAT_WATER);
    int deep_count=0, core_count=0;
    for(int y=15;y<=23;y++)for(int x=15;x<=29;x++) {
        core_count++;deep_count+=features[y][x]==FEAT_DEEP_WATER;
    }
    assert(deep_count>0&&deep_count<core_count);
    for(int x=14;x<=30;x++)
        if(cave_bridge_underlay(features[19][x])==FEAT_DEEP_WATER)
            assert(features[19][x]==FEAT_BRIDGE_DEEP_WATER_H);
    assert(terrain_generation_walkable(17,20,NULL));
    assert(terrain_generation_walkable(19,20,NULL));
    assert(!terrain_generation_jump(17,19,0,1,NULL));
    dump_map("deep-water-bridge");
    reset_map(0);
    for(int x=14;x<=30;x++)features[19][x]=FEAT_WATER;
    terrain_deepen_water();
    for(int x=14;x<=30;x++)assert(features[19][x]==FEAT_WATER);
    /* A wide lake ring must not strand a previously reachable central island. */
    reset_map(0);
    for(int y=12;y<=28;y++)for(int x=12;x<=32;x++)features[y][x]=FEAT_WATER;
    for(int y=18;y<=22;y++)for(int x=20;x<=24;x++)features[y][x]=FEAT_FLOOR;
    terrain_deepen_water();
    for(int y=18;y<=22;y++)for(int x=20;x<=24;x++)
        assert(features[y][x]==FEAT_FLOOR);
    assert(features[12][12]==FEAT_WATER);
    puts("Deep water: shallow banks, deep interiors, traversable movement, bridge underlay, thin rivers and island access: PASS");
}

int main(void) {
    deep_water_tests();
    candidate_tests();
    int maps=0;
    for(int material=0;material<5;material++)for(int shape=0;shape<3;shape++) {
        int generated=0;select_material(material,shape==0?100:0);
        for(unsigned seed=1;seed<=40;seed++) {
            reset_map(shape);state=seed;running_seed=seed;
            place_dungeon_terrain();int count=check_map(material_features[material]);maps++;
            if(count&&generated++==0) {
                char name[100];snprintf(name,sizeof(name),"%s-%s",material_names[material],shape==0?"cavern":shape==1?"labyrinth":"narrow-neck");dump_map(name);
            }
        }
        assert(generated>0);
        printf("%s / shape %d: %d/40 generated, architecture/protected cells/whole-map access intact: PASS\n",material_names[material],shape,generated);
    }
    /* Elemental subtypes take priority over the ordinary depth material. */
    for(int m=2;m<5;m++) {
        select_material(TERRAIN_THEME_WATER,0);int found=0;
        for(unsigned seed=1;seed<=20;seed++) {
            reset_map(0);state=seed;running_seed=seed;current_partition_modes[0]=QUAD_MODE_BIG_CAVE;
            current_partition_big_cave_types[0]=m==2?BIG_CAVE_FIRE:m==3?BIG_CAVE_POIS:BIG_CAVE_ICE;
            place_dungeon_terrain();found+=check_map(material_features[m]);
            for(int y=0;y<40;y++)for(int x=0;x<80;x++)assert(features[y][x]!=FEAT_WATER);
        }assert(found>0);
    }
    /* Touching elemental partitions retain distinct materials. */
    {
        int left=0,right=0;select_material(TERRAIN_THEME_WATER,0);
        for(unsigned seed=1;seed<=20;seed++) {
            reset_map(0);state=seed;running_seed=seed;current_partition_count=current_partition_cols=2;
            current_partition_modes[0]=current_partition_modes[1]=QUAD_MODE_BIG_CAVE;
            current_partition_big_cave_types[0]=BIG_CAVE_FIRE;
            current_partition_big_cave_types[1]=BIG_CAVE_ICE;
            place_dungeon_terrain();check_map(FEAT_LAVA);check_map(FEAT_ICE);
            for(int y=0;y<40;y++)for(int x=0;x<80;x++) {
                if(features[y][x]==FEAT_LAVA){assert(x<40);left++;}
                if(features[y][x]==FEAT_ICE){assert(x>=40);right++;}
                assert(features[y][x]!=FEAT_WATER);
            }
        }assert(left&&right);
    }
    /* Procedural rifts coexist with an existing structural chasm partition. */
    {
        int generated=0;select_material(TERRAIN_THEME_CHASM,0);
        for(unsigned seed=1;seed<=20;seed++) {
            reset_map(0);state=seed;running_seed=seed;current_partition_count=current_partition_cols=2;
            current_partition_modes[0]=QUAD_MODE_CHASM;
            current_partition_modes[1]=QUAD_MODE_CAVEY;
            for(int y=0;y<40;y++)for(int x=0;x<40;x++)info[y][x]|=CAVE_CHASM_AREA;
            place_dungeon_terrain();generated+=check_map(FEAT_CHASM);
            for(int y=0;y<40;y++)for(int x=0;x<40;x++)assert(features[y][x]==original[y][x]);
        }assert(generated>0);
    }
    /* Fully protected terrain has no legal candidate and must return unchanged. */
    reset_map(0);select_material(TERRAIN_THEME_LAVA,100);
    for(int y=0;y<40;y++)for(int x=0;x<80;x++)info[y][x]|=CAVE_ICKY;
    place_dungeon_terrain();assert(!memcmp(features,original,sizeof(features)));
    /* Quest room ownership protects its complete authored rectangle. */
    select_material(TERRAIN_THEME_WATER,100);
    for(unsigned seed=1;seed<=20;seed++) {
        reset_map(0);state=seed;running_seed=seed;dun->is_quest[0]=true;
        place_dungeon_terrain();check_map(FEAT_WATER);
        rectangle b=dun->corner[0];
        for(int y=b.y1;y<=b.y2;y++)for(int x=b.x1;x<=b.x2;x++)assert(features[y][x]==original[y][x]);
    }
    /* Distinct material records prove the actual placement reads current depth. */
    select_material(-1,0);
    for(int depth=3;depth<20;depth++) {
        int generated=0;
        for(unsigned seed=1;seed<=10;seed++) {
            reset_map(0);state=seed;running_seed=seed;player.depth=depth;
            place_dungeon_terrain();generated+=check_map(material_features[depth%5]);
            for(int m=0;m<5;m++)if(m!=depth%5)
                for(int y=0;y<40;y++)for(int x=0;x<80;x++)assert(features[y][x]!=material_features[m]);
        }
        assert(generated>0);
    }
    printf("Local terrain accents (major pass stubbed): %d seeded maps plus subtype and impossible-map checks: PASS\n",maps);
    return 0;
}
