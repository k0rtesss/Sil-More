/* File: fs/savefile-name.c */

#include "angband.h"

#include "blitz.h"
#include "fs/path.h"
#include "fs/savefile-name.h"
#include "log/log.h"

#include <ctype.h>

void process_player_name(bool sf)
{
    int i;

    for (i = 0; op_ptr->full_name[i]; i++)
    {
        char c = op_ptr->full_name[i];

        if (iscntrl((unsigned char)c))
        {
            quit(format("Illegal control char (0x%02X) in player name", c));
        }

        if (iscntrl((unsigned char)c) || c == '/' || c == '\\' || c == ':'
            || c == '*' || c == '?' || c == '"' || c == '<' || c == '>'
            || c == '|')
        {
            c = '_';
        }
        else if (c == ' ')
        {
            c = '_';
        }

        op_ptr->base_name[i] = c;
    }

    op_ptr->base_name[i] = '\0';

    if (!op_ptr->base_name[0])
    {
        log_debug("No base name provided, using 'nameless'");
        SDL_strlcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));
    }

    if (sf)
    {
        char temp[128];

        build_active_savefile_stem(op_ptr->base_name, temp, sizeof(temp));
        path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, temp);
        log_info("Generated savefile path: %s", savefile);
    }
}
