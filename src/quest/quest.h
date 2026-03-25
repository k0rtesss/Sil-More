#ifndef INCLUDED_QUEST_QUEST_H
#define INCLUDED_QUEST_QUEST_H

#include "h-basic.h"

byte quest_get_state(int quest_id);
void quest_set_state(int quest_id, byte state);
int quest_id_for_vala_stage(int vala_id, int stage);
u32b quest_metarun_flag(int quest_id);
cptr quest_display_title(int quest_id);
int quest_completion_cap(int quest_idx);

bool check_quest_eligibility(int quest_idx, int depth);
void apply_quest_rewards(int quest_idx);
void do_cmd_quest_status(void);

cptr* extract_quest_init_texts(int quest_idx, int* count);
cptr* extract_quest_completion_texts(int quest_idx, int* count);
cptr* prepend_repeat_context(int quest_idx, cptr* texts, int* count, bool is_completion);
void free_quest_texts(cptr* texts, int count);
void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color);

void tulkas_quest_interaction(void);
void check_tulkas_quest_interaction(void);
void check_tulkas_quest_completion(int r_idx);
bool tulkas_orc_is_target(int r_idx);
bool tulkas_orc_targets_alive(bool require_unspawned);
void validate_tulkas_quest_on_load(void);

void remove_quest_giver(int quest_giver_r_idx);
bool is_quest_giver_present(int quest_giver_r_idx);
bool spawn_quest_giver_near_player(int quest_giver_r_idx);

void aule_quest_interaction(void);
void check_aule_quest_interaction(void);

void varda_quest_interaction(void);
void check_varda_quest_interaction(void);
void check_varda_quest_completion(int r_idx);

void mandos_quest_interaction(void);
void check_mandos_quest_interaction(void);
void check_mandos_quest_completion(int r_idx);

void niena_quest_interaction(void);
void check_niena_quest_interaction(void);
void check_niena_quest_completion(void);
void check_niena_morgoth_interaction(void);
void niena_mark_morgoth_attack(void);
void niena_revoke_temp_mercy_gift(bool silent);
void ensure_niena_pacifist_active(void);
void ensure_tulkas_morgoth_active(void);
void ensure_varda_ungoliant_active(void);

void check_orome_quest_completion(int r_idx);
void orome_quest_interaction(void);
void check_orome_quest_interaction(void);
void grant_unique_bane_ability(void);
int orome_great_hunt_bit_for_target(int r_idx);

#endif /* INCLUDED_QUEST_QUEST_H */
