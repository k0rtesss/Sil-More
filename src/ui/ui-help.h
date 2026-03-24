/* File: ui-help.h */

#ifndef INCLUDED_UI_HELP_H
#define INCLUDED_UI_HELP_H

#include "h-basic.h"

extern void binding_action_label(int binding, char* buf, size_t buflen);
extern void binding_action_short(int binding, char* buf, size_t buflen);
extern void do_cmd_help(void);

#endif /* INCLUDED_UI_HELP_H */
