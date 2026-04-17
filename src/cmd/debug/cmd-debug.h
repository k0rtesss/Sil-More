#ifndef INCLUDED_CMD_DEBUG_H
#define INCLUDED_CMD_DEBUG_H

#include "h-basic.h"

#ifdef ALLOW_DEBUG
void display_light_map(void);
void display_scent_map(void);
void display_noise_map(void);
void do_cmd_debug(void);
void do_cmd_wiz_unhide(int d);
#endif /* ALLOW_DEBUG */

#ifdef ALLOW_SPOILERS
void do_cmd_spoilers(void);
#endif /* ALLOW_SPOILERS */

#endif /* INCLUDED_CMD_DEBUG_H */
