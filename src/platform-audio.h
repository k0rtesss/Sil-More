#ifndef INCLUDED_PLATFORM_AUDIO_H
#define INCLUDED_PLATFORM_AUDIO_H

#include "sound-config.h"

bool sdl_sound_initialize(void);
void sdl_init_sounds(void);
void sdl_sound_reload(void);
void sdl_sound_shutdown(void);
void sdl_sound_handle(int sound_idx);
struct sound_config* sdl_sound_get_config(void);
void sdl_sound_save_config(void);
void sdl_music_play_main(void);
void sdl_music_play_ambient(void);
void sdl_music_stop_main(void);
void sdl_music_stop_ambient(void);
void sdl_music_update(void);
void sdl_music_update_volumes(void);

#endif /* INCLUDED_PLATFORM_AUDIO_H */
