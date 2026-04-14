#ifndef INCLUDED_PLATFORM_FRAME_H
#define INCLUDED_PLATFORM_FRAME_H

#include "h-basic.h"

struct app_ui_scene;

void platform_frame_process_events(bool wait);
void platform_frame_flush_events(void);
void platform_frame_present(void);
bool platform_frame_render_ui_scene_to_term(int term_index,
    const struct app_ui_scene* scene);
bool platform_frame_render_ui_scene_to_active_term(
    const struct app_ui_scene* scene);
void platform_frame_delay_ms(u32b msec);
void platform_frame_react(void);
void platform_frame_set_active(bool active);
void platform_frame_notify_noise(void);

#endif /* INCLUDED_PLATFORM_FRAME_H */
