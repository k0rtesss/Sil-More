#ifndef INCLUDED_PLAYER_ABILITIES_H
#define INCLUDED_PLAYER_ABILITIES_H

#include "h-basic.h"

int ability_index(int skilltype, int abilitynum);
bool ability_prereqs_met(int skilltype, int abilitynum);
int abilities_in_skill(int skilltype);
bool prereqs(int skilltype, int abilitynum);

#endif /* INCLUDED_PLAYER_ABILITIES_H */
