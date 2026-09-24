#include "angband.h"
/* Exercise the real backend with a dummy audio device, without starting a game. */
#include "../../src/sdl-sound.c"
#include <assert.h>
#include <limits.h>

static maxima limits;
maxima* z_info = &limits;
static player_type player;
player_type* p_ptr = &player;
bool use_sound = true;
cptr ANGBAND_DIR_XTRA = "lib/xtra";
const cptr angband_sound_name[MSG_MAX] = {0};
SDL_IOStream* sdl_fopen(cptr file, cptr mode) { return SDL_IOFromFile(file, mode); }
errr sdl_fclose(SDL_IOStream* stream) { return SDL_CloseIO(stream) ? 0 : -1; }

void log_log(int level, const char* file, int line, const char* fmt, ...)
{
    (void)file; (void)line;
    if (level >= LOG_WARN) {
        va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
        fputc('\n', stderr);
    }
}
size_t strnfmt(char* buf, size_t max, cptr fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int n = SDL_vsnprintf(buf, max, fmt, ap); va_end(ap);
    return n < 0 ? 0 : (size_t)n;
}
static bool monster_entry_has_audio(const monster_sound_entry* entry)
{
    if (!entry) return false;
    for (int i = 0; i < entry->count; ++i)
        if (entry->audio[i]) return true;
    return false;
}
static bool monster_entry_has_file(const monster_sound_entry* entry, const char* needle)
{
    if (!entry) return false;
    for (int i = 0; i < entry->count; ++i)
        if (strstr(entry->files[i], needle)) return true;
    return false;
}
bool path_build(char* buf, size_t max, cptr base, cptr leaf)
{
    return SDL_snprintf(buf, max, "%s/%s", base, leaf) < (int)max;
}
int distance(int y1, int x1, int y2, int x2)
{
    int dy = abs(y1-y2), dx = abs(x1-x2);
    return MAX(dx,dy) + MIN(dx,dy)/2;
}
bool character_generated;
static byte test_features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
byte (*cave_feat)[MAX_DUNGEON_WID] = test_features;
static int test_acoustic_distance = -1;
static int test_river_level, test_still_level, test_lava_level, test_forge_level, test_bridge_level;
static float test_torch_gain;
int cave_audio_distance(int sy, int sx, int ly, int lx)
{
    return test_acoustic_distance >= 0 ? test_acoustic_distance
        : MAX(abs(sy-ly), abs(sx-lx));
}
int cave_flowing_water_sound_level_at(int y, int x) { (void)y; (void)x; return test_river_level; }
int cave_still_liquid_sound_level_at(int y, int x) { (void)y; (void)x; return test_still_level; }
int cave_lava_sound_level_at(int y, int x) { (void)y; (void)x; return test_lava_level; }
int cave_forge_sound_level_at(int y, int x) { (void)y; (void)x; return test_forge_level; }
int cave_bridge_work_sound_level_at(int y, int x) { (void)y; (void)x; return test_bridge_level; }
float cave_fixture_sound_gain_at(int y, int x) { (void)y; (void)x; return test_torch_gain; }
static MIX_Track* last_sfx(void)
{
    return sound_state.sfx_tracks[(sound_state.next_sfx_track
        + SDL_SOUND_MAX_ACTIVE_TRACKS - 1) % SDL_SOUND_MAX_ACTIVE_TRACKS];
}

static void expect_gain(MIX_Track* track, float expected)
{
    assert(SDL_fabsf(MIX_GetTrackGain(track) - expected) < 0.0001f);
}

static void test_stealth_footsteps(void)
{
    assert(sound_footstep_stealth_gain(0) == 1.0f);
    assert(SDL_fabsf(sound_footstep_stealth_gain(5) - 0.9f) < 0.0001f);
    assert(SDL_fabsf(sound_footstep_stealth_gain(-5) - 1.1f) < 0.0001f);
    assert(SDL_fabsf(sound_footstep_stealth_gain(10) - (13.0f / 15.0f)) < 0.0001f);
    assert(SDL_fabsf(sound_footstep_stealth_gain(20) - 0.84f) < 0.0001f);
    assert(sound_footstep_stealth_gain(INT_MAX) >= 0.8f);
    assert(sound_footstep_stealth_gain(INT_MAX) < sound_footstep_stealth_gain(20));
    assert(sound_footstep_stealth_gain(INT_MIN) <= 1.2f);
    assert(sound_footstep_stealth_gain(INT_MIN) > sound_footstep_stealth_gain(-20));

    player.py = player.px = 5;
    test_features[5][5] = FEAT_FLOOR;
    player.skill_use[S_STL] = 5;
    sound(MSG_WALK);
    MIX_Track* dry = last_sfx();
    expect_gain(dry, 0.9f);
    player.px = 6;
    test_features[5][6] = FEAT_WATER;
    sound(MSG_WALK);
    MIX_Track* water = last_sfx();
    expect_gain(water, 0.9f);
    player.skill_use[S_STL] = -5;
    sound(MSG_WALK);
    MIX_Track* loud = last_sfx();
    expect_gain(loud, 1.1f);

    /* Movement updates distance but preserves the emitting step's skill.
     * These assertions catch both a lost modifier and applying it twice. */
    player.px = 8;
    sdl_sound_update_environment();
    SDL_Delay(SOUND_GAIN_RAMP_MS + 80);
    expect_gain(dry, 0.4f);
    expect_gain(water, 0.625f);
    expect_gain(loud, 1.1f * 25.0f / 36.0f);
    sound_at(MSG_WALK, 5, 9); /* another source does not inherit our skill */
    expect_gain(last_sfx(), 1.0f);
    sound(MSG_HIT);
    expect_gain(last_sfx(), 1.0f);
    sound_state.volume_walk = 0.3f;
    sound(MSG_WALK);
    expect_gain(last_sfx(), 0.33f);
    sound_state.volume_walk = 1.0f;

    player.skill_use[S_STL] = 5;
    sound_delayed(MSG_WALK, 80);
    player.skill_use[S_STL] = -5;
    player.px = 9;
    sdl_sound_update_environment();
    SDL_Delay(130);
    expect_gain(last_sfx(), 0.9f);
    assert(!g_delayed_sounds);

    int next = sound_state.next_sfx_track;
    player.leaping = true;
    sound(MSG_WALK);
    assert(next == sound_state.next_sfx_track);
    player.leaping = false;
    sound_state.enable_walk = false;
    sound(MSG_WALK);
    assert(next == sound_state.next_sfx_track);
    sound_state.enable_walk = true;
    player.skill_use[S_STL] = 0;
    player.px = 5;
    MIX_StopAllTracks(sound_state.mixer, 0);
}

static void test_spatial_playback(void)
{
    test_acoustic_distance = -1;
    player.py = player.px = 5;
    player.cur_map_hgt = 40; player.cur_map_wid = 40;
    player.playing = character_generated = true;
    sound_state.volume_combat = sound_state.volume_inventory = sound_state.volume_walk = 1.0f;
    sound_state.enable_river = sound_state.enable_lava = sound_state.enable_torches = true;
    sound_state.enable_forge = sound_state.enable_bridge = true;
    sound_state.volume_river = sound_state.volume_lava = sound_state.volume_torches = 1.0f;
    sound_state.volume_forge = sound_state.volume_bridge = 1.0f;
    MIX_StopAllTracks(sound_state.mixer, 0);
    /* A long decoded recording makes playback assertions independent of the
     * duration of a particular hit variant. Production routing stays intact. */
    sound_state.bank.sound_counts[MSG_HIT] = 1;
    SDL_strlcpy(sound_state.bank.sound_files[MSG_HIT][0],
        "lib/xtra/" RIVER_LOOP_SOUND_PATH, SDL_SOUND_NAME_LEN);
    sound_state.bank.water_walk_count = 1;
    SDL_strlcpy(sound_state.bank.water_walk_files[0],
        "lib/xtra/" RIVER_LOOP_SOUND_PATH, SDL_SOUND_NAME_LEN);
    sound_state.bank.sound_counts[MSG_WALK] = 1;
    SDL_strlcpy(sound_state.bank.sound_files[MSG_WALK][0],
        "lib/xtra/" RIVER_LOOP_SOUND_PATH, SDL_SOUND_NAME_LEN);
    test_stealth_footsteps();
    sound_at(MSG_HIT, 5, 6);
    MIX_Track* near = last_sfx();
    expect_gain(near, 1.0f);
    sound_at(MSG_HIT, 5, 16);
    MIX_Track* far = last_sfx();
    expect_gain(far, 121.0f / 441.0f);
    assert(near != far && MIX_TrackPlaying(near) && MIX_TrackPlaying(far));
    sound_at(MSG_HIT, 5, 26);
    expect_gain(last_sfx(), 1.0f / 441.0f);
    int next = sound_state.next_sfx_track;
    sound_at(MSG_HIT, 5, 27);
    assert(next == sound_state.next_sfx_track);
    monster_type mon = { .r_idx = 21, .fy = 5, .fx = 17, .alertness = ALERTNESS_ALERT };
    monster_sound_force(&mon, MONSTER_SOUND_IDLE);
    assert(next == sound_state.next_sfx_track); /* forcing cannot bypass radius */
    mon.fx = 16;
    monster_sound_force(&mon, MONSTER_SOUND_IDLE);
    expect_gain(last_sfx(), 1.0f / 121.0f);
    /* Water footsteps obey the same gain, using the source tile's material. */
    test_features[5][11] = FEAT_WATER;
    sound_at(MSG_WALK, 5, 11);
    expect_gain(last_sfx(), 1.0f / 36.0f);

    player.px = 15;
    sdl_sound_update_environment();
    SDL_Delay(SOUND_GAIN_RAMP_MS + 80);
    expect_gain(near, 169.0f / 441.0f);
    expect_gain(far, 1.0f);

    /* The timer retains the source and the main thread refreshes queued gain
     * if the listener moves before impact. No dungeon reads on timer threads. */
    player.px = 5;
    sound_delayed_at(MSG_HIT, 80, 5, 16);
    player.px = 15;
    sdl_sound_update_environment();
    SDL_Delay(130);
    expect_gain(last_sfx(), 1.0f);
    assert(!g_delayed_sounds);

    test_river_level = 1; test_still_level = 7;
    test_lava_level = test_forge_level = test_bridge_level = 7;
    test_torch_gain = 1.0f;
    sdl_sound_update_environment();
    SDL_Delay(SOUND_GAIN_RAMP_MS + 80);
    expect_gain(sound_state.river_loop_track, 0.5f / 36.0f);
    expect_gain(sound_state.still_water_loop_track, 0.5f);
    expect_gain(sound_state.torch_loop_track, 0.5f);
    expect_gain(sound_state.lava_loop_track, 0.5f);
    expect_gain(sound_state.forge_loop_track, 0.5f);
    expect_gain(sound_state.bridge_work_loop_track, 0.5f);
    for (int i = 0; i < ENVIRONMENT_LOOP_COUNT; ++i)
        assert(MIX_TrackPlaying(environment_spatial[i].track));
    /* Reversing a fade keeps the same loop running, and repeated refreshes
     * cannot extend a fade forever. Test interpolation without timer races. */
    SDL_LockMutex(g_sound_mutex);
    spatial_track* loop = &environment_spatial[0];
    sdl_sound_target_gain(loop, 0.0f);
    Uint64 started = loop->ramp_start_ms;
    float halfway = sdl_sound_ramp_gain(loop, started + SOUND_GAIN_RAMP_MS / 2);
    assert(halfway > 0.0f && halfway < loop->start_gain);
    sdl_sound_target_gain(loop, 0.0f);
    assert(loop->ramp_start_ms == started);
    sdl_sound_target_gain(loop, 0.5f);
    SDL_UnlockMutex(g_sound_mutex);
    assert(MIX_TrackPlaying(loop->track));
    test_river_level = 0;
    sdl_sound_update_environment();
    SDL_Delay(SOUND_GAIN_RAMP_MS + 80);
    assert(!MIX_TrackPlaying(sound_state.river_loop_track));
    assert(MIX_TrackPlaying(sound_state.still_water_loop_track));
    sound_state.enable_lava = false;
    sdl_sound_update_environment();
    SDL_Delay(SOUND_GAIN_RAMP_MS + 80);
    assert(!MIX_TrackPlaying(sound_state.lava_loop_track));
    assert(MIX_TrackPlaying(sound_state.forge_loop_track));

    sound_delayed_at(MSG_HIT, 50, 5, 16);
    next = sound_state.next_sfx_track;
    sdl_sound_stop_environment();
    SDL_Delay(100);
    assert(sound_state.next_sfx_track == next);
    assert(!g_delayed_sounds);
    for (int i = 0; i < SDL_SOUND_MAX_ACTIVE_TRACKS; ++i)
        assert(!MIX_TrackPlaying(sound_state.sfx_tracks[i]));
    character_generated = false;
}
int main(int argc, char** argv)
{
    assert(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    assert(SDL_Init(SDL_INIT_AUDIO));
    /* Exercise the production JSON writer/reader for all independent choices. */
    const char* config_path = "build-standard/monster-sound-test/config.json";
    struct sound_config saved, restored;
    sound_config_set_defaults(&saved);
    for (int mask = 0; mask < 16; ++mask) {
        saved.enable_attack = (mask & 1) != 0;
        saved.enable_damage = (mask & 2) != 0;
        saved.enable_death = (mask & 4) != 0;
        saved.enable_idle = (mask & 8) != 0;
        sound_config_save(config_path, &saved);
        sound_config_load(config_path, &restored);
        assert(saved.enable_attack == restored.enable_attack);
        assert(saved.enable_damage == restored.enable_damage);
        assert(saved.enable_death == restored.enable_death);
        assert(saved.enable_idle == restored.enable_idle);
    }
    const char* old_config = "{\"enabled\":true,\"enableMonsterHits\":false}";
    assert(SDL_SaveFile(config_path, old_config, strlen(old_config)));
    sound_config_load(config_path, &restored);
    assert(restored.enable_attack && restored.enable_damage
        && restored.enable_death && restored.enable_idle);
    assert(!restored.enable_monster_hits);
    sound_config_set_defaults(&g_sound_config);
    limits.r_max = 403;
    g_sound_config.enabled = true;
    g_sound_config.sample_rate = 44100;
    g_sound_config.channels = 2;
    SDL_strlcpy(g_sound_config.format, "s16", sizeof(g_sound_config.format));
    g_sound_config.volume_master = 1.0f;
    sound_state.enable_monster_hits = true;
    sound_state.volume_monster_hits = 1.0f;
    monster_type mon = {0};
    mon.r_idx = 21; mon.alertness = ALERTNESS_ALERT; mon.fx = 22;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(!g_monster_sounds);
    mon.fx = 1; use_sound = false;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE); assert(!g_monster_sounds);
    use_sound = true; sound_state.enable_monster_hits = false;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE); assert(!g_monster_sounds);
    sound_state.enable_monster_hits = true;
    const int primary_actions[] = {MONSTER_SOUND_MELEE_BASE, MONSTER_SOUND_DAMAGE, MONSTER_SOUND_DEATH};
    for (int i = 0; i < 3; ++i) {
        int action = primary_actions[i];
        monster_sound(&mon, action);
        assert(g_monster_sounds->race_idx == 21);
        assert(g_monster_sounds->action == action);
        assert(g_monster_sounds->count > 0);
        assert(monster_entry_has_audio(g_monster_sounds));
    }
    /* Normal attacks must never select a special attack recording. */
    const int variant_races[] = {21, 104, 32};
    SDL_srand(456);
    for (int r = 0; r < 3; ++r) {
        mon.r_idx = variant_races[r];
        for (int i = 0; i < 100; ++i)
            monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
        monster_sound_entry* entry = g_monster_sounds;
        while (entry && (entry->race_idx != mon.r_idx
            || entry->action != MONSTER_SOUND_MELEE_BASE))
            entry = entry->next;
        assert(entry && entry->count > 0);
        assert(monster_entry_has_audio(entry));
        for (int i = 0; i < entry->count; ++i)
            assert(!strstr(entry->files[i], "Bonus_"));
    }
    /* A stale ranged assignment must not reuse one of the same race's melee
     * recordings, even when the race already has valid ranged sounds. */
    cJSON* test_races = cJSON_GetObjectItemCaseSensitive(
        g_monster_sound_config, "monsters");
    cJSON* test_uldor = cJSON_GetObjectItemCaseSensitive(test_races, "115");
    cJSON* test_melee = cJSON_GetArrayItem(
        cJSON_GetObjectItemCaseSensitive(test_uldor, "melee"), 0);
    cJSON* test_melee_sounds = cJSON_GetObjectItemCaseSensitive(
        test_melee, "sounds");
    cJSON* test_arrow = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(test_uldor, "ranged"), "arrow1");
    assert(test_melee_sounds && test_melee_sounds->child);
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(test_arrow, "sounds"),
        cJSON_Duplicate(test_melee_sounds->child, true));
    mon.r_idx = 115;
    monster_sound(&mon, MONSTER_SOUND_RANGED_BASE);
    assert(g_monster_sounds->race_idx == 115);
    assert(g_monster_sounds->count == 2);
    assert(monster_entry_has_file(g_monster_sounds, "Bow_Attack_1.ogg"));
    mon.r_idx = 71;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(g_monster_sounds->count >= 1);
    assert(monster_entry_has_file(g_monster_sounds, "Spider_Attack.ogg"));
    monster_sound(&mon, (MONSTER_SOUND_RANGED_BASE + 23));
    assert(g_monster_sounds->count >= 1 && monster_entry_has_audio(g_monster_sounds));
    assert(monster_entry_has_file(g_monster_sounds, "Bonus_Spider_Netshot_Attack.ogg"));
    mon.r_idx = 32;
    monster_sound(&mon, (MONSTER_SOUND_RANGED_BASE + 23));
    assert(g_monster_sounds->count == 0);
    /* Assign a web sound explicitly to a different race in memory: no race-ID rule. */
    cJSON* races = cJSON_GetObjectItemCaseSensitive(g_monster_sound_config, "monsters");
    cJSON* attercop = cJSON_GetObjectItemCaseSensitive(races, "71");
    cJSON* web = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(attercop, "ranged"), "throw_web");
    cJSON* grimhawk = cJSON_GetObjectItemCaseSensitive(races, "22");
    cJSON_AddItemToObject(cJSON_GetObjectItemCaseSensitive(grimhawk, "ranged"),
        "throw_web", cJSON_Duplicate(web, true));
    mon.r_idx = 22;
    monster_sound(&mon, MONSTER_SOUND_RANGED_BASE + 23);
    assert(g_monster_sounds->count >= 1 && monster_entry_has_audio(g_monster_sounds));
    assert(monster_entry_has_file(g_monster_sounds, "Netshot"));
    /* Separate slots can select different recordings even on one monster. */
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(grimhawk, "melee"),
        cJSON_Duplicate(web, true));
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(monster_entry_has_file(g_monster_sounds, "Bat_Attack.ogg"));
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE + 1);
    assert(monster_entry_has_audio(g_monster_sounds));
    assert(monster_entry_has_file(g_monster_sounds, "Netshot"));
    /* The other bird family members now have explicit normal attack recordings. */
    mon.r_idx = 43; monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(monster_entry_has_audio(g_monster_sounds));
    mon.r_idx = 63; monster_sound(&mon, MONSTER_SOUND_RANGED_BASE + 8);
    assert(g_monster_sounds->audio[0] && strstr(g_monster_sounds->files[0], "Sonic"));
    mon.r_idx = 12;
    monster_sound(&mon, MONSTER_SOUND_RANGED_BASE);
    assert(g_monster_sounds->count == 0);
    monster_sound_entry* missing = g_monster_sounds;
    monster_sound(&mon, MONSTER_SOUND_RANGED_BASE); assert(g_monster_sounds == missing);
    mon.r_idx = 104;
    monster_sound(&mon, MONSTER_SOUND_DAMAGE);
    assert(monster_entry_has_audio(g_monster_sounds));
    assert(strstr(g_monster_sounds->files[0], "Bat_Damage.ogg"));
    mon.r_idx = 21; mon.fx = 1; test_acoustic_distance = 12;
    monster_sound_entry* before = g_monster_sounds;
    for (int i = 0; i < 1000; ++i) monster_sound(&mon, MONSTER_SOUND_IDLE);
    assert(g_monster_sounds == before);
    test_acoustic_distance = 11;
    mon.alertness = ALERTNESS_UNWARY - 1;
    for (int i = 0; i < 1000; ++i) monster_sound(&mon, MONSTER_SOUND_IDLE);
    assert(g_monster_sounds == before);
    mon.alertness = ALERTNESS_UNWARY; SDL_srand(123);
    int emitted = 0;
    for (int i = 0; i < 1000; ++i) {
        int old = sound_state.next_sfx_track;
        monster_sound(&mon, MONSTER_SOUND_IDLE);
        emitted += old != sound_state.next_sfx_track;
    }
    assert(emitted > 25 && emitted < 75);
    assert(g_monster_sounds->action == MONSTER_SOUND_IDLE);
    assert(g_monster_sounds->audio[0]);
    for (int i = 1; i < argc; ++i) {
        MIX_Audio* audio = MIX_LoadAudio(sound_state.mixer, argv[i], true);
        if (!audio) {
            fprintf(stderr, "Decode failed: %s: %s\n", argv[i], SDL_GetError());
            return 1;
        }
        MIX_DestroyAudio(audio);
    }
    sound_state.enable_combat = true;
    sound_state.enable_inventory = true;
    sound_state.enable_walk = true;
    g_sound_config.enable_attack = false;
    assert(!is_sound_enabled(MSG_SHOOT) && !is_sound_enabled(MSG_WEAPON_SLASH_LIGHT));
    assert(!is_sound_enabled(MSG_MONSTER_ATTACK));
    assert(is_sound_enabled(MSG_HIT) && is_sound_enabled(MSG_KILL));
    assert(is_sound_enabled(MSG_EAT) && is_sound_enabled(MSG_WALK));
    int old_track = sound_state.next_sfx_track;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(sound_state.next_sfx_track == old_track);
    monster_sound(&mon, MONSTER_SOUND_DAMAGE);
    assert(sound_state.next_sfx_track != old_track);
    g_sound_config.enable_attack = true;
    g_sound_config.enable_damage = false;
    assert(!is_sound_enabled(MSG_HIT) && is_sound_enabled(MSG_SHOOT));
    old_track = sound_state.next_sfx_track;
    monster_sound(&mon, MONSTER_SOUND_DAMAGE);
    assert(sound_state.next_sfx_track == old_track);
    g_sound_config.enable_death = false;
    assert(!is_sound_enabled(MSG_KILL) && !is_sound_enabled(MSG_DEATH));
    monster_sound(&mon, MONSTER_SOUND_DEATH);
    assert(sound_state.next_sfx_track == old_track);
    g_sound_config.enable_idle = false;
    for (int i = 0; i < 1000; ++i) monster_sound(&mon, MONSTER_SOUND_IDLE);
    assert(sound_state.next_sfx_track == old_track);
    g_sound_config.enable_damage = g_sound_config.enable_death = g_sound_config.enable_idle = true;
    sdl_sound_shutdown(); assert(!g_monster_sounds);
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(monster_entry_has_audio(g_monster_sounds));
    test_spatial_playback();
    sdl_sound_shutdown(); SDL_Quit();
    printf("PASS: type toggles and persistence, range, mute, per-race playback, missing sounds, idle (%d/1000), "
        "bounded stealth footsteps, spatial gain, moving listener, delayed origins, six-loop mixing/fades, "
        "cache cleanup/recreation; decoded %d OGG files.\n", emitted, argc - 1);
    return 0;
}
