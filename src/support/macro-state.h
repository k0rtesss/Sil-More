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

int macro_find_exact(cptr pat);
int macro_find_check(cptr pat);
int macro_find_maybe(cptr pat);
int macro_find_ready(cptr pat);
errr macro_add(cptr pat, cptr act);
errr macro_init(void);
errr macro_free(void);
errr macro_trigger_free(void);

#endif /* INCLUDED_SUPPORT_MACRO_STATE_H */
