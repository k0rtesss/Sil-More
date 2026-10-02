#!/usr/bin/env python3
"""Check music-cache ownership and settings reloads with real SDL mixer assets."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/audio-review-fixes"

HARNESS = r'''
#include "sdl-sound.c"
#include <assert.h>
#include <stdio.h>

static void assert_title(const char* relative) {
    char path[1024];
    sdl_sound_build_path(relative, path, sizeof(path));
    assert(MIX_TrackPlaying(sound_state.music_main_track));
    assert(streq(sound_state.music_main_current_path, path));
    assert(!MIX_TrackPlaying(sound_state.music_ambient_track));
}
static void assert_silent(void) {
    if (!sound_state.mixer) return;
    assert(!MIX_TrackPlaying(sound_state.music_main_track));
    assert(!MIX_TrackPlaying(sound_state.music_menu_track));
    assert(!MIX_TrackPlaying(sound_state.music_ambient_track));
}
static void save_reload(void) {
    sdl_sound_save_config();
    sdl_sound_reload();
}
static void test_cache(void) {
    assert(MIX_Init());
    sound_state.mixer_initialized = true;
    SDL_AudioSpec spec = {.format=SDL_AUDIO_F32, .channels=1, .freq=48000};
    sound_state.mixer = MIX_CreateMixer(&spec);
    assert(sound_state.mixer && sdl_sound_create_track_pool());
    MIX_Track* tracks[] = {
        sound_state.music_main_track, sound_state.music_menu_track,
        sound_state.music_ambient_track, sound_state.river_loop_track,
        sound_state.still_water_loop_track, sound_state.torch_loop_track,
        sound_state.lava_loop_track, sound_state.forge_loop_track,
        sound_state.bridge_work_loop_track
    };
    const char* assets[] = {
        "music/main.ogg", "music/main_full.ogg", "music/ambient.ogg",
        RIVER_LOOP_SOUND_PATH, STILL_WATER_LOOP_SOUND_PATH, TORCH_LOOP_SOUND_PATH,
        LAVA_LOOP_SOUND_PATH, FORGE_LOOP_SOUND_PATH, BRIDGE_LOOP_WOOD_SOUND_PATH,
        BRIDGE_LOOP_OTHER_SOUND_PATH
    };
    MIX_Audio* attached[9];
    for (int i=0; i<10; ++i) {
        char path[1024]; sdl_sound_build_path(assets[i], path, sizeof(path));
        MIX_Audio* audio = sdl_sound_get_music_audio(path); assert(audio);
        if (i<9) {
            attached[i] = audio;
            assert(sdl_sound_play_track_audio(tracks[i], audio, .1f, -1, false));
        }
    }
    /* Protect stopped/paused inputs too, not only currently playing assets. */
    assert(MIX_PauseTrack(tracks[1]));
    assert(MIX_StopTrack(tracks[2], 0));
    char death[1024]; sdl_sound_build_path("music/death.ogg", death, sizeof(death));
    assert(sdl_sound_get_music_audio(death));
    for (int i=0; i<9; ++i) assert(MIX_GetTrackAudio(tracks[i]) == attached[i]);
    assert(MIX_TrackPaused(tracks[1]));
    assert(!MIX_TrackPlaying(tracks[2]));
    float mixed[4800];
    assert(MIX_Generate(sound_state.mixer, mixed, sizeof(mixed)) > 0);
    for (int i=0; i<9; ++i) assert(MIX_GetAudioDuration(attached[i]) > 0);
    /* Repeated replacements and a failed load must not poison the cache. */
    char bridge[1024]; sdl_sound_build_path(BRIDGE_LOOP_OTHER_SOUND_PATH, bridge, sizeof(bridge));
    for (int i=0; i<20; ++i) {
        assert(sdl_sound_get_music_audio(i%2 ? death : bridge));
        assert(!sdl_sound_get_music_audio("missing-audio-review-file.ogg"));
        for (int j=0; j<9; ++j) assert(MIX_GetTrackAudio(tracks[j]) == attached[j]);
    }
    sound_state.music_main_enabled = sound_state.music_ambient_enabled = true;
    sound_state.music_main_volume = sound_state.music_ambient_volume = 1;
    SDL_strlcpy(sound_state.music_death_path, death, sizeof(sound_state.music_death_path));
    sdl_music_play_death(); assert_title("music/death.ogg");
    puts("PASS: 11 shipped cues, repeated cache replacement, failed-load recovery, and playing/paused/stopped input ownership.");
    sdl_sound_shutdown();
}
static void test_reload(void) {
    sound_config_set_defaults(&g_sound_config);
    g_sound_config.enabled = true;
    sound_config_save(g_sound_config_path, &g_sound_config);
    sdl_sound_reload(); assert_silent();
    sdl_music_play_ambient();
    assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    g_sound_config.volume_walk = .9f;
    save_reload();
    assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    assert(!MIX_TrackPlaying(sound_state.music_main_track));
    sdl_music_update(); assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    puts("PASS: changing unrelated walking volume preserves persistent ambient music after settings reload.");

    sdl_music_play_menu_theme();
    assert(MIX_TrackPlaying(sound_state.music_menu_track));
    save_reload();
    assert(MIX_TrackPlaying(sound_state.music_menu_track));
    assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    sdl_music_stop_main(); /* Actual options exit: clear overlay, keep ambient. */
    assert(!MIX_TrackPlaying(sound_state.music_menu_track));
    assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    puts("PASS: reload preserves active menu overlay; returning to the game retains only ambient.");

    void (*play[])(void) = {sdl_music_play_main, sdl_music_play_main_full, sdl_music_play_death};
    const char* paths[] = {"music/main.ogg", "music/main_full.ogg", "music/death.ogg"};
    for (int i=0; i<3; ++i) {
        play[i](); assert_title(paths[i]);
        save_reload(); assert_title(paths[i]);
        g_sound_config.music_main_enabled = false;
        save_reload(); assert_silent();
        g_sound_config.music_main_enabled = true;
        save_reload(); assert_title(paths[i]);
    }
    puts("PASS: title/full-title/death selection survives reload and individual music disable/re-enable.");

    sdl_music_play_ambient();
    g_sound_config.music_ambient_enabled = false;
    save_reload(); assert_silent();
    g_sound_config.music_ambient_enabled = true;
    save_reload(); assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    g_sound_config.enabled = false;
    save_reload(); assert(!sound_state.mixer);
    g_sound_config.enabled = true;
    save_reload(); assert(MIX_TrackPlaying(sound_state.music_ambient_track));
    puts("PASS: ambient and master disable/re-enable restore the requested enabled cue without playing while disabled.");

    sdl_music_stop_ambient(); save_reload(); assert_silent();
    sdl_music_play_death(); sdl_music_stop_main(); save_reload(); assert_silent();
    g_sound_config.enabled = false; save_reload();
    g_sound_config.enabled = true; save_reload(); assert_silent();
    puts("PASS: explicit stops remain stopped across settings reload and master toggles.");

    /* Full-title intent survives the existing fallback to main. */
    SDL_strlcpy(g_sound_config.music_main_full_path, "music/missing.ogg", sizeof(g_sound_config.music_main_full_path));
    sdl_sound_save_config(); sdl_music_play_main_full(); assert_title("music/main.ogg");
    save_reload(); assert_title("music/main.ogg");
    SDL_strlcpy(g_sound_config.music_main_full_path, "music/main_full.ogg", sizeof(g_sound_config.music_main_full_path));
    save_reload(); assert_title("music/main_full.ogg");
    sdl_sound_shutdown(); sdl_sound_reload(); assert_silent();
    sdl_sound_shutdown();
    puts("PASS: fallback retains full-title intent; shutdown clears playback intent before fresh initialization.");
}
int main(int argc, char** argv) {
    assert(argc == 4);
    ANGBAND_DIR_XTRA = argv[1]; ANGBAND_DIR_USER = argv[2]; ANGBAND_DIR_PREF = argv[3];
    assert(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    assert(SDL_Init(SDL_INIT_AUDIO));
    sound_config_set_defaults(&g_sound_config); g_sound_config.enabled = true;
    test_cache();
    snprintf(g_sound_config_path, sizeof(g_sound_config_path), "%s/sound.json", argv[2]);
    test_reload(); SDL_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_mixer", "SDL_image", "SDL_ttf")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith(("/src/main.c.obj", "/src/sdl-sound.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O1",
        "-ffunction-sections", "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    result = subprocess.run([
        str(exe), str(ROOT / "lib/xtra"), str(OUT), str(ROOT / "lib/pref")],
        cwd=OUT, env=env, text=True, capture_output=True, timeout=60)
    (OUT / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    if result.returncode:
        print(result.stderr)
        result.check_returncode()


if __name__ == "__main__":
    main()
