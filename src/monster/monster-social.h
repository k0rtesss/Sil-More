#ifndef INCLUDED_MONSTER_SOCIAL_H
#define INCLUDED_MONSTER_SOCIAL_H

/* Zero means infer membership from the race. Custom bands can mix races. */
enum monster_group {
    MON_GROUP_AUTO, MON_GROUP_OTHER, MON_GROUP_ORC, MON_GROUP_MAN,
    MON_GROUP_ELF, MON_GROUP_THRALL, MON_GROUP_TROLL, MON_GROUP_RAUKO,
    MON_GROUP_CUSTOM_FIRST, MON_GROUP_MAX = 16
};
enum monster_relation { MON_REL_NEUTRAL, MON_REL_HOSTILE };
enum monster_social_state {
    MON_SOCIAL_NONE, MON_SOCIAL_CHALLENGE, MON_SOCIAL_FIGHT,
    MON_SOCIAL_HELP, MON_SOCIAL_AVOID
};

#define MON_SOCIAL_MEMORY 20
#define MON_SOCIAL_COOLDOWN 80
#define MON_SOCIAL_SAVE_MAGIC 0x5019
#define MON_SOCIAL_DISPUTE_ACTIONS 16
#define MON_SOCIAL_RESPONSE_ACTIONS 8
#define MON_SOCIAL_RECORD_BYTES 12

int monster_social_group(const monster_type* m);
bool monster_social_set_group(monster_type* m, int group);
int monster_group_relation(int a, int b);
bool monster_group_set_relation(int a, int b, int relation);
int monster_social_relation(const monster_type* a, const monster_type* b);
bool monster_social_allies(const monster_type* a, const monster_type* b);
bool monster_social_start_feud(int a, int b);
void monster_social_player_attack(monster_type* m);
void monster_social_reset(void);
void monster_social_remap(int old_idx, int new_idx);
void monster_social_tick(monster_type* m);
bool monster_social_turn(monster_type* m);
bool monster_social_valid(const monster_type* m);

#endif
