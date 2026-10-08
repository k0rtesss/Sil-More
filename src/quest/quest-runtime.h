#ifndef SIL_QUEST_RUNTIME_H
#define SIL_QUEST_RUNTIME_H

#include "angband.h"
#include "quest/quest-options.h"

byte quest_get_state(int id);
void quest_set_state(int id, byte state);
u32b quest_metarun_flag(int id);
int quest_completion_cap(int id);
cptr quest_display_title(int id);
int quest_id_for_vala_stage(int vala, int stage);
bool quest_followup_eligible(int id, int depth);
bool quest_followup_interaction(int giver);
void quest_followup_update(void);
void quest_followup_kill(int race);
void quest_followup_damage(monster_type *monster, int damage, int who);
void quest_followup_escape(void);
void quest_followup_leave(int new_depth);
void quest_followup_discard_level(void);
void quest_followup_reset(void);
bool quest_debug_sandbox(void);
void quest_debug_start(int id);
void quest_debug_complete(int id);
void quest_debug_reset(int id);
void quest_debug_status(int id, char *buf, size_t size);
int quest_debug_vault_requested(void);
void quest_debug_request_vault(int id);
bool quest_debug_prepare_vault(int id);
bool quest_varda_radiant_gift(void);
void varda_quest_begin_player_turn(void);
int quest_followup_vault(int vault);
bool quest_followup_vault_allowed(int vault, int depth);
void quest_followup_vault_placed(int id, int depth);
bool quest_followup_reserve_race(int race);
void quest_followup_generation_begin(void);
void quest_followup_generation_commit(void);
void quest_followup_generate_giver(void);
bool quest_special_ability_active(int ability);
bool quest_challenge_active(int id);
int quest_challenge_completion_count(int id);
bool quest_challenge_unlocked(int id);
void quest_challenge_choose(void);
void quest_challenge_record_escape(void);

#endif
