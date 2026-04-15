#ifndef INCLUDED_PLATFORM_FRAME_H
#define INCLUDED_PLATFORM_FRAME_H

#include "h-basic.h"

struct app_ui_scene;

void platform_frame_process_events(bool wait);
void platform_frame_flush_events(void);
void platform_frame_present(void);
bool platform_frame_render_ui_scene_to_view(int view_index,
    const struct app_ui_scene* scene);
bool platform_frame_render_ui_scene_to_active_view(
    const struct app_ui_scene* scene);
bool platform_frame_view_ready(int view_index);
const char* platform_frame_view_name(int view_index);
int platform_frame_active_view_index(void);
void platform_frame_set_active_view(int view_index);
bool platform_frame_main_view_ready(void);
int platform_frame_main_grid_cols(void);
int platform_frame_main_grid_rows(void);
int platform_frame_active_grid_cols(void);
int platform_frame_active_grid_rows(void);
bool platform_frame_active_view_is_main(void);
void platform_frame_shutdown_views(void);
void platform_frame_delay_ms(u32b msec);
void platform_frame_react(void);
void platform_frame_set_active(bool active);
void platform_frame_notify_noise(void);

#endif /* INCLUDED_PLATFORM_FRAME_H */
