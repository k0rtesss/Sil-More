#ifndef INCLUDED_UI_DEATH_H
#define INCLUDED_UI_DEATH_H

#include "h-basic.h"

struct high_score;

void do_cmd_morgoth_victory(void);
void ui_death_show_character_info(void);
int ui_death_final_menu(const struct high_score* score, int* highlight);

#endif /* INCLUDED_UI_DEATH_H */
