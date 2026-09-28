/* Included by the full-production linked pipeline harness. Profiles are forced
 * through linker wrappers; shape construction and validation remain production. */
static bool landmark_fixture_active;
static terrain_theme_profile fixture_theme;
static terrain_landmark_profile fixture_landmark;
static terrain_network_profile fixture_network;
static const char* fixture_output;
static int fixture_debug_seed;
const terrain_network_profile* __real_terrain_network_for_depth(int depth);
const terrain_network_profile* __wrap_terrain_network_for_depth(int depth) {
    return landmark_fixture_active?&fixture_network:__real_terrain_network_for_depth(depth);
}
const terrain_theme_profile* __real_terrain_theme_for_depth(int depth);
const terrain_theme_profile* __wrap_terrain_theme_for_depth(int depth) {
    return landmark_fixture_active?&fixture_theme:__real_terrain_theme_for_depth(depth);
}
const terrain_landmark_profile* __real_terrain_landmark_for_depth(int depth);
const terrain_landmark_profile* __wrap_terrain_landmark_for_depth(int depth) {
    return landmark_fixture_active?&fixture_landmark:__real_terrain_landmark_for_depth(depth);
}

static void landmark_fixture_reset(int architecture) {
    memset(dun,0,sizeof(*dun));
    memset(room_anchor_kind,0,sizeof(room_anchor_kind));
    memset(cave_escape_tunnel,0,sizeof(cave_escape_tunnel));
    cave_fixtures_clear();
    terrain_vault_reset();
    styles_set_loaded_level_primary(0);
    mon_max=o_max=1;
    p_ptr->cur_map_hgt=66;p_ptr->cur_map_wid=132;p_ptr->depth=10;
    p_ptr->py=8;p_ptr->px=8;
    morgoth_level_active=morgoth_partition_reserved=qv_placed_this_level=false;
    current_partition_rows=current_partition_cols=2;current_partition_count=4;
    for(int pi=0;pi<4;pi++) {
        current_partition_modes[pi]=architecture?QUAD_MODE_LABYRINTH:QUAD_MODE_CAVEY;
        current_partition_big_cave_types[pi]=BIG_CAVE_NONE;
    }
    for(int y=0;y<MAX_DUNGEON_HGT;y++)for(int x=0;x<MAX_DUNGEON_WID;x++) {
        cave_feat[y][x]=FEAT_WALL_EXTRA;cave_info[y][x]=0;
        cave_color[y][x]=COLOR_STYLE_BASE;
        cave_natural[y][x]=0;cave_o_idx[y][x]=cave_m_idx[y][x]=0;
        cave_corridor1[y][x]=cave_corridor2[y][x]=0;
    }
    for(int y=4;y<62;y++)for(int x=4;x<128;x++) {
        bool floor=!architecture || y%12<4 || x%20<4;
        if(floor) {
            cave_feat[y][x]=FEAT_FLOOR;
            cave_info[y][x]=architecture?0:CAVE_ROOM;
            cave_natural[y][x]=!architecture;
            if(architecture)cave_corridor1[y][x]=1;
        }
    }
    /* Protected anchors are deliberately scattered across likely paths. */
    const coord critical[]={{12,20},{24,40},{36,60},{48,80},{12,100}};
    const int features[]={FEAT_MORE,FEAT_LESS,FEAT_FORGE_NORMAL_HEAD,FEAT_GLYPH,FEAT_WALL_PERM};
    for(int i=0;i<5;i++)cave_feat[critical[i].y][critical[i].x]=features[i];
    dun->cent_n=4;
    for(int i=0;i<4;i++) {
        dun->cent[i]=(coord){i<2?12:48,i%2?104:20};
        room_anchor_kind[i]=architecture?LAYOUT_ANCHOR_BSP_SLICE:LAYOUT_ANCHOR_CA_BLOB;
        dun->corner[i]=(rectangle){i<2?4:34,i%2?68:4,i<2?30:61,i%2?127:63};
    }
    terrain_generation_reset();terrain_landmark_reset();
}

static void landmark_fixture_contents(void) {
    int race=1;
    while(race<z_info->r_max && (!r_info[race].name || (r_info[race].flags1&RF1_UNIQUE)))race++;
    assert(race<z_info->r_max);
    for(int i=0;i<8;i++) {
        int y=22+(i/4)*16,x=44+(i%4)*12;
        cave_feat[y][x]=cave_feat[y+1][x]=FEAT_FLOOR;
        monster_type* m=&mon_list[mon_max];memset(m,0,sizeof(*m));
        m->r_idx=race;m->fy=y;m->fx=x;m->hp=m->maxhp=10;
        cave_m_idx[y][x]=mon_max++;
        object_type* o=&o_list[o_max];memset(o,0,sizeof(*o));
        o->k_idx=1;o->tval=TV_FOOD;o->number=1;o->iy=y+1;o->ix=x;
        cave_o_idx[y+1][x]=o_max++;
    }
}

static void landmark_fixture_vault(int policy) {
    int serial=1;
    while(serial<z_info->v_max && !(v_info[serial].flags&VLT_TERRAIN_FLOOD))serial++;
    assert(serial<z_info->v_max);
    vault_type* vault=&v_info[serial];u32b original_flags=vault->flags;
    int original_type=vault->typ;
    vault->typ=policy>=4?policy+4:7;
    vault->flags&=~(VLT_TERRAIN_FLOOD|VLT_TERRAIN_CROSSING|VLT_TERRAIN_REPAIRED|VLT_QUEST);
    if(policy==1)vault->flags|=VLT_TERRAIN_REPAIRED;
    if(policy==2)vault->flags|=VLT_TERRAIN_FLOOD;
    if(policy==3)vault->flags|=VLT_QUEST;
    terrain_vault_begin();
    for(int y=18;y<=46;y++)for(int x=36;x<=96;x++) {
        bool wall=y==18||y==46||x==36||x==96;
        cave_feat[y][x]=wall?FEAT_WALL_OUTER:FEAT_FLOOR;
        cave_info[y][x]|=CAVE_ICKY|CAVE_ROOM;
        terrain_vault_record(y,x,vault,wall?'#':'.');
    }
    cave_feat[32][36]=cave_feat[32][96]=FEAT_FLOOR;
    terrain_vault_record(32,36,vault,'.');terrain_vault_record(32,96,vault,'.');
    cave_feat[32][66]=FEAT_FORGE_NORMAL_HEAD;terrain_vault_record(32,66,vault,'0');
    cave_feat[24][66]=FEAT_MORE;terrain_vault_record(24,66,vault,'>');
    if(policy==3) {
        int i=dun->cent_n++;
        dun->cent[i]=(coord){32,66};dun->corner[i]=(rectangle){18,36,46,96};
        dun->is_quest[i]=true;room_anchor_kind[i]=LAYOUT_ANCHOR_PREFAB;
    }
    vault->flags=original_flags;vault->typ=original_type;
}

static bool landmark_fixture_hydraulic(int y,int x) {
    int cell=terrain_landmark_cell(y,x);
    return cell==TERRAIN_LANDMARK_TERRAIN||cell==TERRAIN_LANDMARK_BRIDGE;
}
static bool landmark_fixture_rock(int feat) {
    return feat==FEAT_QUARTZ || feat==FEAT_WALL_EXTRA || feat==FEAT_WALL_OUTER
        || feat==FEAT_WALL_INNER || feat==FEAT_WALL_SOLID || feat==FEAT_WALL_PERM;
}
static void landmark_fixture_endpoints(void) {
    bool basin_core[256]={0};int terminal_count=0;
    for(int y=2;y<64;y++)for(int x=2;x<130;x++) {
        int basin=terrain_landmark_basin_cell(y,x);if(basin<=0||basin>=256)continue;
        bool core=true;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
            core&=landmark_fixture_hydraulic(y+dy,x+dx);
        if(core)basin_core[basin]=true;
    }
    for(int y=1;y<65;y++)for(int x=1;x<131;x++) {
        int terminal=terrain_landmark_terminal_cell(y,x);if(!terminal)continue;
        terminal_count++;assert(landmark_fixture_hydraulic(y,x));
        const int materials[]={FEAT_WATER,FEAT_CHASM,FEAT_LAVA,FEAT_POISON,FEAT_ICE};
        assert(cave_feat[y][x]==materials[terrain_landmark_last_stats()->material]);
        if(terminal==TERRAIN_TERMINAL_EDGE)assert(y==1||y==64||x==1||x==130);
        else if(terminal==TERRAIN_TERMINAL_BASIN) {
            int basin=terrain_landmark_basin_cell(y,x);
            assert(basin>0&&basin<256&&basin_core[basin]);
        } else if(terminal==TERRAIN_TERMINAL_VENT) {
            assert(cave_feat[y][x]==FEAT_LAVA);
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)
                assert(cave_feat[y+dy][x+dx]==FEAT_LAVA);
            bool rim=false;
            for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)
                if(in_bounds_fully(y+dy,x+dx))rim|=landmark_fixture_rock(cave_feat[y+dy][x+dx]);
            assert(rim);
        } else {
            assert(terminal==TERRAIN_TERMINAL_SPRING||terminal==TERRAIN_TERMINAL_SINK
                ||terminal==6); /* Fissure tip shares a wall-backed pinch. */
            bool wall=false;int horizontal=1,vertical=1,neighbors=0;
            const int dy[]={-1,0,1,0},dx[]={0,1,0,-1};
            for(int d=0;d<4;d++) {
                wall|=landmark_fixture_rock(cave_feat[y+dy[d]][x+dx[d]]);
                neighbors+=landmark_fixture_hydraulic(y+dy[d],x+dx[d]);
            }
            for(int xx=x-1;xx>0&&landmark_fixture_hydraulic(y,xx);xx--)horizontal++;
            for(int xx=x+1;xx<131&&landmark_fixture_hydraulic(y,xx);xx++)horizontal++;
            for(int yy=y-1;yy>0&&landmark_fixture_hydraulic(yy,x);yy--)vertical++;
            for(int yy=y+1;yy<65&&landmark_fixture_hydraulic(yy,x);yy++)vertical++;
            assert(wall&&MIN(horizontal,vertical)==1&&neighbors==1);
        }
    }
    assert(terminal_count>=2);
}

static int landmark_fixture_shape(void) {
    static byte seen[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static coord queue[MAX_DUNGEON_HGT*MAX_DUNGEON_WID];
    memset(seen,0,sizeof(seen));
    int area=0,largest=0,miny=MAX_DUNGEON_HGT,minx=MAX_DUNGEON_WID,maxy=0,maxx=0;
    bool partitions[25]={0};int partition_count=0;
    for(int y=1;y<65;y++)for(int x=1;x<131;x++) {
        int cell=terrain_landmark_cell(y,x);
        if(cell!=TERRAIN_LANDMARK_TERRAIN && cell!=TERRAIN_LANDMARK_BRIDGE)continue;
        area++;miny=MIN(miny,y);maxy=MAX(maxy,y);minx=MIN(minx,x);maxx=MAX(maxx,x);
        int pi=level_partition_index_for_point(y,x);assert(pi>=0&&pi<4);partitions[pi]=true;
        if(cell==TERRAIN_LANDMARK_BRIDGE)assert(cave_feat_is_bridge(cave_feat[y][x]));
        else {
            const int features[]={FEAT_WATER,FEAT_CHASM,FEAT_LAVA,FEAT_POISON,FEAT_ICE};
            assert(cave_feat[y][x]==features[terrain_landmark_last_stats()->material]);
        }
        if(seen[y][x])continue;
        int head=0,tail=0;queue[tail++]=(coord){y,x};seen[y][x]=1;
        while(head<tail) {
            coord p=queue[head++];
            const int dy[]={-1,0,1,0},dx[]={0,1,0,-1};
            for(int d=0;d<4;d++) {
                int yy=p.y+dy[d],xx=p.x+dx[d],v=terrain_landmark_cell(yy,xx);
                if(!in_bounds_fully(yy,xx)||seen[yy][xx]
                    ||(v!=TERRAIN_LANDMARK_TERRAIN&&v!=TERRAIN_LANDMARK_BRIDGE))continue;
                seen[yy][xx]=1;queue[tail++]=(coord){yy,xx};
            }
        }
        largest=MAX(largest,tail);
    }
    for(int pi=0;pi<4;pi++)partition_count+=partitions[pi];
    const terrain_landmark_stats* s=terrain_landmark_last_stats();
    assert(area>0);
    landmark_fixture_endpoints();
    for(int y=1;y<65;y++)for(int x=1;x<131;x++) {
        int cell=terrain_landmark_cell(y,x);
        if(cell==TERRAIN_LANDMARK_RUBBLE) {
            assert(cave_feat[y][x]==FEAT_RUBBLE);
            assert(landmark_fixture_rock(terrain_before[y][x]));
            assert(!cave_m_idx[y][x]&&!cave_o_idx[y][x]);
        }
        if(cell==TERRAIN_LANDMARK_REPAIR)assert(cave_feat[y][x]==FEAT_FLOOR);
    }
    if(s->family!=2 && s->family!=3) {
        assert((maxy-miny+1)*100>=66*fixture_landmark.min_span_percent
            ||(maxx-minx+1)*100>=132*fixture_landmark.min_span_percent);
        assert(partition_count>=2);assert(largest*4>=area*3);
    }
    /* Inspect rasterized geometry, independently of requested basin labels.
     * A core survives one-cell Chebyshev erosion; thin connectors do not. */
    static byte core[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    memset(core,0,sizeof(core));memset(seen,0,sizeof(seen));
    int core_count=0,smallest_core=100000,largest_core=0,channel_count=0,narrow_count=0,junction_count=0;
    for(int y=2;y<64;y++)for(int x=2;x<130;x++) {
        bool interior=true;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++) {
            int v=terrain_landmark_cell(y+dy,x+dx);
            if(v!=TERRAIN_LANDMARK_TERRAIN&&v!=TERRAIN_LANDMARK_BRIDGE)interior=false;
        }
        core[y][x]=interior;
        if(terrain_landmark_channel_cell(y,x)) {
            channel_count++;
            junction_count+=(terrain_landmark_channel_cell(y-1,x)
                +terrain_landmark_channel_cell(y+1,x)+terrain_landmark_channel_cell(y,x-1)
                +terrain_landmark_channel_cell(y,x+1))>=3;
            int h=1,v=1;
            for(int xx=x-1;xx>0&&(terrain_landmark_cell(y,xx)==TERRAIN_LANDMARK_TERRAIN
                ||terrain_landmark_cell(y,xx)==TERRAIN_LANDMARK_BRIDGE);xx--)h++;
            for(int xx=x+1;xx<131&&(terrain_landmark_cell(y,xx)==TERRAIN_LANDMARK_TERRAIN
                ||terrain_landmark_cell(y,xx)==TERRAIN_LANDMARK_BRIDGE);xx++)h++;
            for(int yy=y-1;yy>0&&(terrain_landmark_cell(yy,x)==TERRAIN_LANDMARK_TERRAIN
                ||terrain_landmark_cell(yy,x)==TERRAIN_LANDMARK_BRIDGE);yy--)v++;
            for(int yy=y+1;yy<65&&(terrain_landmark_cell(yy,x)==TERRAIN_LANDMARK_TERRAIN
                ||terrain_landmark_cell(yy,x)==TERRAIN_LANDMARK_BRIDGE);yy++)v++;
            narrow_count+=MIN(h,v)==1;
        }
    }
    for(int y=2;y<64;y++)for(int x=2;x<130;x++) {
        if(!core[y][x]||seen[y][x])continue;
        int head=0,tail=0;queue[tail++]=(coord){y,x};seen[y][x]=1;
        while(head<tail) {
            coord p=queue[head++];const int dy[]={-1,0,1,0},dx[]={0,1,0,-1};
            for(int d=0;d<4;d++) {
                int yy=p.y+dy[d],xx=p.x+dx[d];
                if(core[yy][xx]&&!seen[yy][xx]) {seen[yy][xx]=1;queue[tail++]=(coord){yy,xx};}
            }
        }
        if(tail>=3) {core_count++;smallest_core=MIN(smallest_core,tail);largest_core=MAX(largest_core,tail);}
    }
    if(s->family==0) {
        if(core_count<2||largest_core<=smallest_core) {
            printf("SHAPE FAILURE family=%d case=%d cores=%d smallest=%d largest=%d\n",
                s->family,fixture_debug_seed,core_count,smallest_core,largest_core);
            export_map(fixture_output,s->material+1,fixture_debug_seed,0);
        }
        assert(core_count>=2);assert(largest_core>smallest_core);
    }
    if(s->family==2||s->family==3)assert(core_count>0);
    if(s->family==3)assert(core_count>=2);
    if(s->family==0||s->family==1) {assert(channel_count>=3);assert(narrow_count*100>=channel_count*40);}
    if(s->family==1)assert(junction_count>0);
    return area;
}

static void run_landmark_fixtures(const char* out) {
    fixture_output=out;
    landmark_fixture_active=true;
    fixture_theme=(terrain_theme_profile){0};fixture_theme.chance=0;
    fixture_theme.min_length=12;fixture_theme.max_length=24;fixture_theme.max_width=3;
    fixture_landmark=(terrain_landmark_profile){100,60,85,1,3,0,25};
    fixture_network=(terrain_network_profile){0};
    fixture_network.min_basins=2;fixture_network.max_basins=4;
    fixture_network.min_radius=3;fixture_network.max_radius=7;
    fixture_network.one_tile_percent=70;
    int accepted_total=0,bridged_total=0,lake_total=0,relocated_total=0;
    for(int family=0;family<5;family++)for(int architecture=0;architecture<2;architecture++)for(int material=0;material<5;material++) {
        /* Chasm always has its dedicated fracture family. */
        if(material==TERRAIN_THEME_CHASM&&family)continue;
        memset(fixture_network.weights,0,sizeof(fixture_network.weights));fixture_network.weights[family]=1;
        memset(fixture_theme.weights,0,sizeof(fixture_theme.weights));fixture_theme.weights[material]=1;
        fixture_landmark.lake_chance=architecture?0:100;
        int accepted=0;
        for(int seed=1;seed<=16;seed++) {
            landmark_fixture_reset(architecture);landmark_fixture_contents();Rand_state_init((u64b)seed);
            if(!__wrap_place_terrain_landmark())continue;
            const terrain_landmark_stats* s=terrain_landmark_last_stats();
            int area=landmark_fixture_shape();
            assert(s->material==material);
            assert(s->family==(material==TERRAIN_THEME_CHASM?5:family));
            for(int y=1;y<65;y++)for(int x=1;x<131;x++)
                if(critical_before[y][x])assert(terrain_before[y][x]==cave_feat[y][x]);
            if(!accepted) {
                generation_attempts=1;export_map(out,material+1,10000+1000*family+100*architecture+seed,0);
            }
            accepted++;accepted_total++;
            bridged_total+=s->bridges>0;lake_total+=s->kind==TERRAIN_LANDMARK_LAKE;
            relocated_total+=s->relocated_monsters+s->relocated_objects;
            printf("LANDMARK FIXTURE family=%d material=%d architecture=%d seed=%d area=%d span=%d bridges=%d\n",
                s->family,material,architecture,seed,area,s->span,s->bridges);
        }
        assert(accepted>=8); /* At least half of these intentionally favorable maps. */
    }
    assert(bridged_total>0&&lake_total>0);
    assert(relocated_total>0);
    int flooded_vaults=0,crossed_vaults=0,quest_changed=0,ruined_walls=0,repaired_vaults=0;
    int policy_changes[7]={0};
    memset(fixture_theme.weights,0,sizeof(fixture_theme.weights));
    memset(fixture_network.weights,0,sizeof(fixture_network.weights));fixture_network.weights[0]=1;
    fixture_theme.weights[TERRAIN_THEME_WATER]=1;fixture_landmark.lake_chance=100;
    for(int policy=0;policy<7;policy++)for(int seed=1;seed<=24;seed++) {
        fixture_debug_seed=90000+100*policy+seed;
        landmark_fixture_reset(0);landmark_fixture_vault(policy);
        landmark_fixture_contents();Rand_state_init((u64b)seed);
        if(!__wrap_place_terrain_landmark())continue;
        landmark_fixture_shape();
        int changed=0,walls=0,changed_walls=0;
        for(int y=18;y<=46;y++)for(int x=36;x<=96;x++) {
            changed+=terrain_before[y][x]!=cave_feat[y][x];
            if(terrain_before[y][x]>=FEAT_WALL_HEAD&&terrain_before[y][x]<=FEAT_WALL_TAIL) {
                walls++;changed_walls+=terrain_before[y][x]!=cave_feat[y][x];
            }
        }
        if(policy>=4)assert(!changed);
        else policy_changes[policy]+=changed>0;
        if(!policy)crossed_vaults+=changed>0;
        if(policy==1)repaired_vaults+=terrain_landmark_last_stats()->repaired_structures;
        if(policy==2) {flooded_vaults+=changed>0;ruined_walls+=changed_walls;}
        if(policy==3)quest_changed+=changed>0;
        if(changed && policy_changes[policy]==1) {
            generation_attempts=1;export_map(out,1,20000+100*policy+seed,0);
        }
        assert(cave_feat[32][66]==FEAT_FORGE_NORMAL_HEAD&&cave_feat[24][66]==FEAT_MORE);
    }
    assert(flooded_vaults>0&&crossed_vaults>0&&quest_changed>0&&ruined_walls>0);
    assert(repaired_vaults>0);
    int lava_ruins=0;
    memset(fixture_theme.weights,0,sizeof(fixture_theme.weights));
    fixture_theme.weights[TERRAIN_THEME_LAVA]=1;
    for(int seed=1;seed<=24;seed++) {
        landmark_fixture_reset(0);landmark_fixture_vault(2);
        landmark_fixture_contents();Rand_state_init((u64b)seed);
        if(!__wrap_place_terrain_landmark())continue;
        landmark_fixture_shape();
        int destroyed=0;
        for(int y=18;y<=46;y++)for(int x=36;x<=96;x++)
            destroyed+=landmark_fixture_rock(terrain_before[y][x])&&cave_feat[y][x]==FEAT_LAVA;
        if(destroyed) {
            lava_ruins++;
            if(lava_ruins==1) {generation_attempts=1;export_map(out,3,21000+seed,0);}
        }
    }
    assert(lava_ruins>0);
    memset(fixture_theme.weights,0,sizeof(fixture_theme.weights));
    fixture_theme.weights[TERRAIN_THEME_WATER]=1;
    int elemental_accepted=0,elemental_granite=0;
    static byte elemental_core[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    const int cave_types[]={BIG_CAVE_FIRE,BIG_CAVE_POIS,BIG_CAVE_ICE};
    for(int subtype=0;subtype<3;subtype++)for(int seed=1;seed<=64;seed++) {
        landmark_fixture_reset(0);
        current_partition_modes[0]=QUAD_MODE_BIG_CAVE;
        current_partition_big_cave_types[0]=cave_types[subtype];
        for(int y=1;y<65;y++)for(int x=1;x<131;x++)
            elemental_core[y][x]=level_partition_index_for_point(y,x)==0
                && (cave_info[y][x]&CAVE_ROOM);
        /* Forced ordinary water must route around incompatible elemental cave
         * floor while remaining a half-level feature across other partitions. */
        Rand_state_init((u64b)seed);
        if(!__wrap_place_terrain_landmark())continue;
        elemental_accepted++;landmark_fixture_shape();
        for(int y=1;y<65;y++)for(int x=1;x<131;x++) {
            /* Newly excavated granite gains CAVE_ROOM on commit. Only the
             * original elemental footprint is protected against other liquids. */
            if(elemental_core[y][x])
                assert(terrain_before[y][x]==cave_feat[y][x]);
            else if(level_partition_index_for_point(y,x)==0
                && terrain_before[y][x]==FEAT_WALL_EXTRA
                && cave_feat[y][x]!=FEAT_WALL_EXTRA)elemental_granite++;
        }
    }
    assert(elemental_accepted>=24);
    assert(elemental_granite>0);
    /* A protected map rejects atomically instead of accepting a tiny fallback. */
    landmark_fixture_reset(0);
    for(int y=0;y<66;y++)for(int x=0;x<132;x++)cave_feat[y][x]=FEAT_WALL_PERM;
    Rand_state_init(2);assert(!__wrap_place_terrain_landmark());
    assert(!memcmp(terrain_before,cave_feat,sizeof(terrain_before)));
    assert(!terrain_landmark_last_stats()->accepted);
    landmark_fixture_active=false;
    printf("LANDMARK FIXTURES PASS accepted=%d bridged=%d lakes=%d relocated=%d flooded_vaults=%d crossed_vaults=%d quest_changed=%d repaired_vaults=%d ruined_walls=%d lava_ruins=%d elemental=%d elemental_granite=%d\n",
        accepted_total,bridged_total,lake_total,relocated_total,flooded_vaults,crossed_vaults,quest_changed,repaired_vaults,ruined_walls,lava_ruins,elemental_accepted,elemental_granite);
}
