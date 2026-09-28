#ifndef INCLUDED_TUTORIAL_GAME_H
#define INCLUDED_TUTORIAL_GAME_H

#include "tutorial.h"

/* Integration boundary: callers pass already-known information only. */
void tutorial_game_start(void);
/* Generation uses this before tutorial_game_start() to protect a fresh
 * turn-zero opening; it reads the selected mode and Tale history only. */
bool tutorial_game_start_needs_clear_area(void);
/* Uses already-computed monster visibility; never queues or displays a card. */
bool tutorial_game_first_monster_triggered(void);
/* Uses already-computed visibility; covers every map subject that the first
 * gameplay checkpoint can observe, without queuing or displaying a card. */
bool tutorial_game_first_turn_triggered(void);
void tutorial_game_checkpoint(void);
void tutorial_game_wait(void);
void tutorial_game_menu(const char *id, const char *description);
void tutorial_game_explain(const char *id, const char *subject,
    const char *description);
void tutorial_game_explain_now(const char *id, const char *subject,
    const char *description);
void tutorial_game_item(const object_type *item);
void tutorial_game_item_described(const object_type *item);
void tutorial_game_item_description_closed(void);
void tutorial_game_item_used(const object_type *item);
void tutorial_game_identified(const object_type *item, const char *reason);
void tutorial_game_ability(int skill, int ability, bool before_purchase);
void tutorial_game_attack(const monster_type *target);
void tutorial_game_combat_roll(const combat_roll *roll);
void tutorial_game_lifecycle(const char *id);
bool tutorial_game_command_allowed(int command, int direction);
bool tutorial_game_action_allowed(const char *action, const object_type *item);
bool tutorial_game_target_allowed(int y, int x);
void tutorial_game_action_done(const char *action, const object_type *item);
bool tutorial_game_begin_action(const char *action, const object_type *item);
void tutorial_game_end_action(void);
bool tutorial_game_select_mode(void);
void tutorial_game_settings(void);
void tutorial_game_archive(void);

#endif
