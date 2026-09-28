#ifndef INCLUDED_SUPPORT_FEEDBACK_H
#define INCLUDED_SUPPORT_FEEDBACK_H

#include "h-basic.h"

void bell(cptr reason);
void sound(int val);
/* Use the actual source grid for all effects away from the player. */
void sound_at(int val, int y, int x);
void sound_delayed(int val, unsigned int delay_ms);
void sound_delayed_at(int val, unsigned int delay_ms, int y, int x);

#endif /* INCLUDED_SUPPORT_FEEDBACK_H */
