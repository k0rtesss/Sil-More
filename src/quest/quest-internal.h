#ifndef SIL_QUEST_INTERNAL_H
#define SIL_QUEST_INTERNAL_H

#include "angband.h"

int select_tulkas_quest_target(void);
int select_tulkas_quest_target_for_depth(int depth);
int select_tulkas_quest_prize(int target_level);
bool tulkas_target_race_valid(const monster_race* r_ptr);
u32b get_metarun_quest_flag(int quest_idx);
int get_quest_oath_id(int quest_idx);
cptr get_oath_name_from_id(byte oath_id);
cptr* prepend_repeat_context(
    int quest_idx, cptr* texts, int* count, bool is_completion);
char* my_strstr(const char* haystack, const char* needle);
void display_wrapped_text(
    int col, int* row, cptr text, byte color, int max_width);
void quest_minimum_summary(int quest_idx, bool complete, char* out, size_t size);
void quest_minimum_dialogue(int quest_idx, bool complete, cptr title,
    cptr texts[], int count, byte title_color, byte text_color);
void quest_minimum_custom_dialogue(cptr title, cptr summary, cptr texts[],
    int count, byte title_color, byte text_color, int pages);
/* Keep the ordinary dialogue path intact when the presentation option is off. */
#define QUEST_PRESENT_DIALOGUE(id, done, title, texts, count, tc, bc) \
    do { \
        if (minimum_text_mode) \
            quest_minimum_dialogue((id), (done), (title), (texts), (count), (tc), (bc)); \
        else \
            quest_typewriter_menu((title), (texts), (count), (tc), (bc)); \
    } while (0)
void remove_quest_giver_silent(int quest_giver_r_idx);
bool trigger_adjacent_quest_giver_interaction(
    int quest_giver_r_idx, cptr quest_giver_name,
    void (*interaction)(void));
bool ensure_reward_quest_giver_near_player(
    int quest_giver_r_idx, int radius, cptr quest_giver_name,
    cptr arrival_message, int* spawn_y, int* spawn_x);

#endif
