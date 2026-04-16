#ifndef INCLUDED_PLAYER_OATHS_H
#define INCLUDED_PLAYER_OATHS_H

#include "h-basic.h"

extern char* oath_name[];

bool oath_invalid(int oath_id);
void apply_oath_breaking_curse(int oath_id);
bool chosen_oath(int oath_id);
char* oath_confirmation_prompt(int oath_id);
char* oath_curse_message(int oath_id);
char* oath_permanent_message(int oath_id);
char* oath_death_message(int oath_id);
char* oath_banned_text(int oath_id);
char* oath_name_str(int oath_id);
char* oath_description(int oath_id);
char* oath_pledge(int oath_id);
char* oath_forbidden(int oath_id);
char* oath_reward_text(int oath_id);

#endif /* INCLUDED_PLAYER_OATHS_H */
