/* File: fs/save-notes-inventory.c -- carved from save.c (shares state via fs/save-internal.h) */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "fs/save-internal.h"
#include <stdio.h>

/*
 * Write the notes into the savefile. Every savefile has at least NOTES_MARK.
 */
void wr_notes(void)
{
    size_t i;

    /* Preserve the existing NUL-separated record format without copying lines
     * through a fixed-size stack buffer.  A full notes buffer may end mid-line. */
    for (i = 0; i < sizeof(notes_buffer) && notes_buffer[i]; i++)
        wr_byte(notes_buffer[i] == '\n' ? 0 : (byte)notes_buffer[i]);
    if (i && notes_buffer[i - 1] != '\n')
        wr_byte(0);

    /* Always write NOTES_MARK */
    wr_string(NOTES_MARK);
}

