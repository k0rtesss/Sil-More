#ifndef INCLUDED_PLATFORM_AUDIO_H
#define INCLUDED_PLATFORM_AUDIO_H

#include "sound-config.h"

bool platform_sound_initialize(void);
void platform_sound_init(void);
void platform_sound_reload(void);
void platform_sound_shutdown(void);
void platform_sound_handle(int sound_idx);
struct sound_config* platform_sound_config(void);
void platform_sound_save_config(void);
void platform_music_play_main(void);
void platform_music_play_main_full(void);
void platform_music_play_menu_theme(void);
void platform_music_play_ambient(void);
void platform_music_play_death(void);
void platform_music_request_welcome_main_once(void);
bool platform_music_consume_welcome_main_once(void);
void platform_music_stop_main(void);
void platform_music_stop_ambient(void);
void platform_music_update(void);
void platform_music_update_volumes(void);

#endif /* INCLUDED_PLATFORM_AUDIO_H */
