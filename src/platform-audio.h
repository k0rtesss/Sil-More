/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

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
