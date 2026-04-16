#ifndef INCLUDED_SUPPORT_MACRO_STATE_H
#define INCLUDED_SUPPORT_MACRO_STATE_H

#include "h-basic.h"

extern s16b macro__num;
extern cptr* macro__pat;
extern cptr* macro__act;

extern int max_macrotrigger;
extern cptr macro_template;
extern cptr macro_modifier_chr;
extern cptr macro_modifier_name[MAX_MACRO_MOD];
extern cptr macro_trigger_name[MAX_MACRO_TRIGGER];
extern cptr macro_trigger_keycode[2][MAX_MACRO_TRIGGER];

extern char macro_buffer[1024];
extern cptr keymap_act[KEYMAP_MODES][256];

#endif /* INCLUDED_SUPPORT_MACRO_STATE_H */
