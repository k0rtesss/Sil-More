/* File: ui/ui-character-name.c */

#include "angband.h"
#include "externs.h"

#include "fs/savefile-name.h"
#include "log/log.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-character-name.h"

#define INSTRUCT_ROW 21
#define QUESTION_COL 2

bool get_name(void)
{
    char tmp[14];
    char old_name[14];

    log_info("Starting character name selection process");

    tmp[0] = '\0';
    old_name[0] = '\0';

    display_player(0);

    Term_putstr(
        QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_SLATE, "Enter accept name");
    Term_putstr(
        QUESTION_COL, INSTRUCT_ROW + 2, -1, TERM_SLATE, "  Tab random name");

    Term_putstr(QUESTION_COL, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "Enter");
    Term_putstr(QUESTION_COL + 2, INSTRUCT_ROW + 2, -1, TERM_L_WHITE, "Tab");

    if (character_dungeon)
    {
        Term_putstr(QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_SLATE,
            "ESC abort name change                  ");
        Term_putstr(
            QUESTION_COL + 38 + 2, INSTRUCT_ROW + 1, -1, TERM_L_WHITE, "ESC");
    }

    SDL_strlcpy(tmp, c_name + c_info[p_ptr->pcharacter].name, sizeof(tmp));
    SDL_strlcpy(old_name, c_name + c_info[p_ptr->pcharacter].name,
        sizeof(old_name));

    Term_gotoxy(8, 2);

    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name,
        sizeof(op_ptr->full_name));
    process_player_name(true);

    log_info("Character name confirmed: '%s'", op_ptr->full_name);

    return true;
}
