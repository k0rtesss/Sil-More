#pragma once

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "sound-config.h"

/* Initialize the SDL sound subsystem and load the sound registry. */
bool sdl_sound_initialize(void);

/* Reload sound configuration data (safe to call multiple times). */
void sdl_sound_reload(void);

/* Release any audio resources owned by the SDL sound subsystem. */
void sdl_sound_shutdown(void);

/* Play the sound mapped to the specified Angband message index. */
void sdl_sound_handle(int sound_idx);
/* Play a one-shot with the distance gain used by environmental loops. */
void sdl_sound_handle_at_environment_level(int sound_idx, int level,
    int max_level);
void sdl_sound_monster(int race_idx, int action);
/* Play a monster action without the normal idle-cue probability roll. */
void sdl_sound_monster_force(int race_idx, int action);

/* Update/stop positional environmental loops used during dungeon play. */
void sdl_sound_update_environment(void);
void sdl_sound_stop_environment(void);

/* Schedule a sound to play after delay_ms milliseconds without blocking the
 * caller. Playback starts from the timer callback through SDL_mixer's
 * thread-safe API, independent of UI event pumping. */
void sdl_sound_handle_delayed(int sound_idx, Uint32 delay_ms);

/* Consume a delayed-sound timing diagnostic event, if applicable. */
bool sdl_sound_try_handle_event(const SDL_Event* ev);

/* Get pointer to global sound configuration (for UI) */
struct sound_config* sdl_sound_get_config(void);

/* Save sound configuration to disk and update internal state */
void sdl_sound_save_config(void);

/* Music control functions */
void sdl_music_play_main(void);
void sdl_music_play_main_full(void);
void sdl_music_play_death(void);
void sdl_music_play_menu_theme(void);
void sdl_music_play_ambient(void);
void sdl_music_stop_main(void);
void sdl_music_stop_ambient(void);
void sdl_music_update(void); /* Call periodically to handle music looping */
void sdl_music_update_volumes(void); /* Update volume of currently playing music */
void sdl_music_request_welcome_main_once(void);
bool sdl_music_consume_welcome_main_once(void);
