/* File: cmd-ui.h */

/*
 * Transitional public header for the UI command split.
 */

#ifndef INCLUDED_CMD_UI_H
#define INCLUDED_CMD_UI_H

#include "h-basic.h"

typedef struct supply_menu_request supply_menu_request;

#define KNOWLEDGE_PAGE_ARTEFACTS 0
#define KNOWLEDGE_PAGE_OBJECTS 1
#define KNOWLEDGE_PAGE_MONSTERS 2
#define KNOWLEDGE_PAGE_CURSES 3

int cmd_ui_knowledge_last_page(void);
void controller_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
void do_cmd_redraw(void);
void do_cmd_version(void);
void do_cmd_feeling(void);
void do_cmd_change_song(void);
void do_cmd_character_sheet(void);
void do_cmd_main_menu(void);
void do_cmd_message_one(void);
void do_cmd_messages(void);
void do_cmd_options(void);
void do_cmd_options_aux(int page, cptr info);
void do_cmd_target(void);
void do_cmd_look(void);
void do_cmd_look_at(int y, int x);
void do_cmd_unified_look(void);
void do_cmd_locate(void);
void do_cmd_query_symbol(void);
void do_cmd_view_monsters(void);
void do_cmd_view_objects(void);
void highlight_entity_on_map(int y, int x, bool highlight);
void highlight_entity_on_map_type(int y, int x, bool highlight,
    int entity_type);
void do_cmd_ability_screen(void);
void do_cmd_note(char* note, int what_depth);
void do_cmd_knowledge_notes(void);
void do_cmd_knowledge_browser_page(int page);
void do_cmd_knowledge_oaths(void);
void do_cmd_knowledge_artefacts(void);
void do_cmd_knowledge_monsters(void);
void do_cmd_knowledge_objects(void);
void do_cmd_knowledge_kills(void);
void do_cmd_knowledge(void);
bool ang_sort_comp_hook(const void* u, const void* v, int a, int b);
void ang_sort_swap_hook(void* u, void* v, int a, int b);
void ghost_challenge(void);
void desc_art_fake(int a_idx);
void apply_magic_fake(object_type* o_ptr);
void add_random_curse(object_type* o_ptr);
bool do_cmd_knowledge_supplies(const supply_menu_request* request);

#endif /* INCLUDED_CMD_UI_H */
