#ifndef INCLUDED_PLATFORM_FRAME_H
#define INCLUDED_PLATFORM_FRAME_H

#include "h-basic.h"

void platform_frame_process_events(bool wait);
void platform_frame_flush_events(void);
void platform_frame_present(void);
void platform_frame_delay_ms(u32b msec);
void platform_frame_react(void);
void platform_frame_set_active(bool active);
void platform_frame_notify_noise(void);

#endif /* INCLUDED_PLATFORM_FRAME_H */
