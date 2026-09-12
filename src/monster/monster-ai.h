#ifndef MONSTER_AI_H
#define MONSTER_AI_H

/* Positive defensive evidence means the player was difficult to affect.
 * Reaction evidence records an action actually demonstrated, never equipment. */
enum monster_ai_feature
{
    MON_AI_FIRE, MON_AI_COLD, MON_AI_POISON, MON_AI_DARK,
    MON_AI_FEAR, MON_AI_SLOW, MON_AI_CONFUSION, MON_AI_HOLD,
    MON_AI_WEB, MON_AI_DISARM, MON_AI_FLANKING, MON_AI_CONTROLLED_RETREAT,
    MON_AI_OPPORTUNIST, MON_AI_ZONE, MON_AI_POLEARM, MON_AI_RIPOSTE,
    MON_AI_CHARGE, MON_AI_KNOCKBACK, MON_AI_EXCHANGE, MON_AI_KITING,
    MON_AI_FOCUS, MON_AI_CONCENTRATION, MON_AI_MULTI_TARGET, MON_AI_STEALTH,
    MON_AI_SONG, MON_AI_ACCURACY, MON_AI_ARMOUR, MON_AI_WOUNDED,
    MON_AI_POISON_PRESSURE,
    /* Append only: MULTI_TARGET is a retired, ambiguous .6 save slot. */
    MON_AI_IMPALE, MON_AI_WHIRLWIND, MON_AI_FOLLOW_THROUGH, MON_AI_SLAY_FEAR
};

bool monster_ai_enabled(const monster_type* m_ptr);
bool monster_ai_can_see_player(const monster_type* m_ptr);
int monster_ai_confidence(const monster_type* m_ptr, int feature);
void monster_ai_observe(monster_type* m_ptr, int feature, int evidence);
void monster_ai_witness(int feature, int evidence, int y, int x);
void scare_onlooking_friends_from_weapon(const monster_type* m_ptr, int amount);
void monster_ai_player_attack(monster_type* target, int attack_type);
void monster_ai_player_action(void);
void monster_ai_share_warning(monster_type* source);
void monster_ai_begin_turn(monster_type* m_ptr);
void monster_ai_end_turn(monster_type* m_ptr, int old_y, int old_x, bool skipped);
void monster_ai_reset(monster_type* m_ptr);
void monster_ai_sanitize(monster_type* m_ptr);

#endif
