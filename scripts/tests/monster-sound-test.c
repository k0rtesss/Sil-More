#include "angband.h"
/* Exercise the real backend with a dummy audio device, without starting a game. */
#include "../../src/sdl-sound.c"
#include <assert.h>

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
bool path_build(char* buf, size_t max, cptr base, cptr leaf)
{
    return SDL_snprintf(buf, max, "%s/%s", base, leaf) < (int)max;
}
int distance(int y1, int x1, int y2, int x2)
{
    int dy = abs(y1-y2), dx = abs(x1-x2);
    return MAX(dx,dy) + MIN(dx,dy)/2;
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
    mon.r_idx = 21; mon.alertness = ALERTNESS_ALERT; mon.fx = 21;
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
    mon.r_idx = 71;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(g_monster_sounds->count == 1);
    assert(strstr(g_monster_sounds->files[0], "Spider_Attack.ogg"));
    monster_sound(&mon, (MONSTER_SOUND_RANGED_BASE + 23));
    assert(g_monster_sounds->count == 1 && g_monster_sounds->audio[0]);
    assert(strstr(g_monster_sounds->files[0], "Bonus_Spider_Netshot_Attack.ogg"));
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
    assert(g_monster_sounds->count == 1 && g_monster_sounds->audio[0]);
    assert(strstr(g_monster_sounds->files[0], "Netshot"));
    /* Separate slots can select different recordings even on one monster. */
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(grimhawk, "melee"),
        cJSON_Duplicate(web, true));
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(strstr(g_monster_sounds->files[0], "Bat_Attack.ogg"));
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE + 1);
    assert(g_monster_sounds->audio[0] && strstr(g_monster_sounds->files[0], "Netshot"));
    /* The other bird family members now have explicit normal attack recordings. */
    mon.r_idx = 43; monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(monster_entry_has_audio(g_monster_sounds));
    mon.r_idx = 63; monster_sound(&mon, MONSTER_SOUND_RANGED_BASE + 8);
    assert(g_monster_sounds->audio[0] && strstr(g_monster_sounds->files[0], "Sonic"));
    mon.r_idx = 12;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE);
    assert(g_monster_sounds->count == 0);
    monster_sound_entry* missing = g_monster_sounds;
    monster_sound(&mon, MONSTER_SOUND_MELEE_BASE); assert(g_monster_sounds == missing);
    mon.r_idx = 104;
    monster_sound(&mon, MONSTER_SOUND_DAMAGE);
    assert(monster_entry_has_audio(g_monster_sounds));
    assert(strstr(g_monster_sounds->files[0], "Bat_Damage.ogg"));
    mon.r_idx = 21; mon.fx = 11;
    monster_sound_entry* before = g_monster_sounds;
    for (int i = 0; i < 1000; ++i) monster_sound(&mon, MONSTER_SOUND_IDLE);
    assert(g_monster_sounds == before);
    mon.fx = 10; mon.alertness = ALERTNESS_UNWARY - 1;
    for (int i = 0; i < 1000; ++i) monster_sound(&mon, MONSTER_SOUND_IDLE);
    assert(g_monster_sounds == before);
    mon.alertness = ALERTNESS_UNWARY; SDL_srand(123);
    int emitted = 0;
    for (int i = 0; i < 1000; ++i) {
        int old = sound_state.next_sfx_track;
        monster_sound(&mon, MONSTER_SOUND_IDLE);
        emitted += old != sound_state.next_sfx_track;
    }
    assert(emitted > 5 && emitted < 50);
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
    sdl_sound_shutdown(); SDL_Quit();
    printf("PASS: type toggles and persistence, range, mute, per-race playback, missing sounds, idle (%d/1000), "
        "cache cleanup/recreation; decoded %d OGG files.\n", emitted, argc - 1);
    return 0;
}
