#ifndef INCLUDED_METARUN_UI_H
#define INCLUDED_METARUN_UI_H

#include "../metarun.h"
#include "app/app-ui.h"

cptr metarun_curse_display_name(int idx);
cptr metarun_blessing_display_name(int idx);
void metarun_present_story_texts(const char* title, cptr texts[],
    int total_texts, byte title_attr, byte text_attr,
    const char* action_label);
void metarun_trim_first_line(char* dst, size_t dst_size,
    const char* source);
bool metarun_show_completed_quests_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label);
void metarun_choose_difficulty_menu(bool reopen_stats_on_exit);
bool metarun_list_history_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label);
bool metarun_show_known_curses_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label);
app_ui_panel* metarun_ui_begin_browser_scene(app_ui_scene* scene,
    byte title_attr, const char* title, byte subtitle_attr,
    const char* subtitle);
bool metarun_ui_add_section_row(app_ui_panel* panel, byte attr,
    const char* text);
bool metarun_ui_add_value_row(app_ui_panel* panel, byte label_attr,
    const char* label, byte value_attr, const char* value);
bool metarun_ui_add_effect_row_ex(app_ui_panel* panel, int id,
    bool selected);
bool metarun_ui_add_effect_row(app_ui_panel* panel, int id);
app_ui_panel* metarun_ui_begin_story_scene(app_ui_scene* scene,
    byte title_attr, const char* title);
bool metarun_ui_present_scene(app_ui_scene* scene, bool fade_in);
void metarun_ui_clear_pending_input(void);
bool metarun_ui_add_wrapped_detail_lines(app_ui_panel* panel, byte attr,
    const char* text);
bool metarun_ui_add_story_paragraphs(app_ui_scene* scene,
    app_ui_panel* panel, const char* const* paragraphs, const byte* attrs,
    int paragraph_count);
bool metarun_ui_add_effect_detail_lines(app_ui_panel* panel, int id);
bool metarun_ui_add_known_curse_detail_lines(app_ui_panel* panel, int id);
bool metarun_ui_show_notice_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count,
    bool steamdeck, const char* accept_label);
bool metarun_ui_show_story_modal(const char* title, byte title_attr,
    const char* const* paragraphs, const byte* attrs, int paragraph_count,
    bool steamdeck, const char* accept_label, const char* action_label);
bool metarun_ui_confirm_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count,
    bool steamdeck, const char* accept_label, const char* back_label);
int metarun_ui_choose_curse_scene(int n, const int* picks, bool steamdeck,
    const char* accept_label);

#endif /* INCLUDED_METARUN_UI_H */
