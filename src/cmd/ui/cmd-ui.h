/* File: cmd-ui.h */

/*
 * Transitional public header for the UI command split.
 */

#ifndef INCLUDED_CMD_UI_H
#define INCLUDED_CMD_UI_H

#include "h-basic.h"

int cmd_ui_knowledge_last_page(void);
void controller_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
void do_cmd_note(char* note, int what_depth);

#endif /* INCLUDED_CMD_UI_H */
