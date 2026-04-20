/*
 * Copyright (c) 1997 Ben Harrison
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "blitz.h"
#include "fs/file.h"
#include "fs/path.h"
#include "log/log.h"
#include "support/reliability-checks.h"
#include "score/score_guid.h"

#include "h-define.h"
#include "init.h"
#include "init2-internal.h"

maxima* z_info;

vault_type* v_info;
char* v_name;
char* v_text;

feature_type* f_info;
char* f_name;
char* f_text;

object_kind* k_info;
char* k_name;
char* k_text;

ability_type* b_info;
char* b_name;
char* b_text;

artefact_type* a_info;
char* a_text;
bool* valar_reserved_artifacts;

ego_item_type* e_info;
char* e_name;
char* e_text;

monster_race* r_info;
monster_race* r_base;
char* r_name;
char* r_text;

player_race* p_info;
char* p_name;
char* p_text;

character_profile* c_info;
char* c_name;
char* c_text;

hist_type* h_info;
char* h_text;

story_type* st_info;
char* st_text;
char* st_name;

curse_type* cu_info;
char* cu_text;
char* cu_name;

major_blessing_type* mb_info;
char* mb_text;
char* mb_name;

quest_type* quest_info;
char* quest_name_text;
char* quest_desc_text;
char* q_text;

oath_type* oath_info;
char* oath_name_text;
char* oath_desc_text;

flavor_type* flavor_info;
char* flavor_name;
char* flavor_text;

names_type* n_info;
style_type* style_info;
char* style_name;
skeleton_note_template* skeleton_note_info;
char* skeleton_note_text;

byte misc_to_attr[256];
char misc_to_char[256];

runtype_type* runtype_info = NULL;

#ifdef ALLOW_TEMPLATES

/*
 * Hack -- help give useful error messages
 */
int error_idx;
int error_line;

/*
 * Standard error message text
 */
static cptr err_str[PARSE_ERROR_MAX] = {
    NULL,
    "parse error",
    "obsolete file",
    "missing record header",
    "non-sequential records",
    "invalid flag specification",
    "undefined directive",
    "out of memory",
    "value out of bounds",
    "too few arguments",
    "too many arguments",
    "too many allocation entries",
    "invalid spell frequency",
    "invalid number of items (0-99)",
    "too many entries",
    "vault too big",
    "vault not rectangular (check spaces at end of line?)",
    NULL,
};

#endif /* ALLOW_TEMPLATES */

/*
 * File headers
 */
header z_head;
header v_head;
header f_head;
header k_head;
header b_head;
header a_head;
header e_head;
header r_head;
header p_head;
header c_head;
header h_head;
header st_head;
header cu_head;
header mb_head;
header b_head;
header g_head;
header flavor_head;
header quest_head;
header oath_head;
header n_head;
header style_head;
header skeleton_note_head;

/*** Initialize from binary image files ***/

/*
 * Initialize a "*_info" array, by parsing a binary "image" file
 */
static errr init_info_raw(ang_file* fd, header* head)
{
    header test;
    Sint64 file_size_64;
    size_t file_size;
    size_t expected_size = 0;
    reliability_layout_status layout_status;

    /* Read and verify the header */
    if (sdl_read(fd, (char*)(&test), sizeof(header))
        || (test.v_major != head->v_major) || (test.v_minor != head->v_minor)
        || (test.v_patch != head->v_patch) || (test.v_extra != head->v_extra)
        || (test.info_num != head->info_num)
        || (test.info_len != head->info_len)
        || (test.head_size != head->head_size)
        || (test.info_size != head->info_size))
    {
        /* Error */
        return (-1);
    }

    /* Accept the header */
    memcpy(head, &test, sizeof(header));

    file_size_64 = sdl_size(fd);
    if (file_size_64 < 0)
        return (-1);

    file_size = (size_t)file_size_64;
    layout_status = reliability_validate_serialized_layout(file_size,
        (size_t)test.head_size, (size_t)test.info_size, (size_t)test.name_size,
        (size_t)test.text_size, &expected_size);
    if (layout_status != RELIABILITY_LAYOUT_VALID)
    {
        log_warn("init_info_raw: rejecting corrupt raw cache (file_size=%zu expected=%zu status=%d)",
            file_size, expected_size, (int)layout_status);
        return (-1);
    }

    /* Allocate the "*_info" array */
    head->info_ptr = mem_alloc_array(head->info_size, char);
    if (!head->info_ptr)
        return (-1);

    /* Read the "*_info" array */
    if (sdl_read(fd, head->info_ptr, head->info_size))
        goto fail;

    if (head->name_size)
    {
        /* Allocate the "*_name" array */
        head->name_ptr = mem_alloc_array(head->name_size, char);
        if (!head->name_ptr)
            goto fail;

        /* Read the "*_name" array */
        if (sdl_read(fd, head->name_ptr, head->name_size))
            goto fail;
    }

    if (head->text_size)
    {
        /* Allocate the "*_text" array */
        head->text_ptr = mem_alloc_array(head->text_size, char);
        if (!head->text_ptr)
            goto fail;

        /* Read the "*_text" array */
        if (sdl_read(fd, head->text_ptr, head->text_size))
            goto fail;
    }

    /* Success */
    return (0);

fail:
    mem_free_null(head->text_ptr);
    mem_free_null(head->name_ptr);
    mem_free_null(head->info_ptr);
    return (-1);
}

/*
 * Initialize the header of an *_info.raw file.
 */
void init_header(header* head, int num, int len)
{
    /* Save the "version" */
    head->v_major = VERSION_MAJOR;
    head->v_minor = VERSION_MINOR;
    head->v_patch = VERSION_PATCH;
    head->v_extra = VERSION_EXTRA;

    /* Save the "record" information */
    head->info_num = num;
    head->info_len = len;

    /* Save the size of "*_head" and "*_info" */
    head->head_size = sizeof(header);
    head->info_size = head->info_num * head->info_len;
}

#ifdef ALLOW_TEMPLATES

/*
 * Display a parser error message.
 */
void display_parse_error(cptr filename, errr err, cptr buf)
{
    cptr oops;

    /* Error string */
    oops = (((err > 0) && (err < PARSE_ERROR_MAX)) ? err_str[err] : "unknown");

    /* Oops */
    msg_format("Error at line %d of '%s.txt'.", error_line, filename);
    msg_format("Record %d contains a '%s' error.", error_idx, oops);
    msg_format("Parsing '%s'.", buf);

    /* Explicitly log the error to log.txt as requested (one line) */
    log_error("CRITICAL PARSE ERROR: %s in %s.txt at line %d (record %d). Entry: '%s'",
        oops, filename, error_line, error_idx, buf);

    message_flush();

    /* Quit */
    quit(format("Error in '%s.txt' file.", filename));
}

#endif /* ALLOW_TEMPLATES */

/*
 * Initialize a "*_info" array
 */
static errr init_info(cptr filename, header* head)
{
    ang_file* fd;

    errr err = 1;

    ang_file* fp;

    /* General buffer */
    char buf[1024];

#ifdef ALLOW_TEMPLATES

    /*** Load the binary image file ***/

    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

    /* Attempt to open the "raw" file */
    fd = ang_file_open(buf, "rb");

    /* Process existing "raw" file */
    if (fd)
    {
#ifdef CHECK_MODIFICATION_TIME
        /* Check if text file is newer than raw file */
        char txt_path[1024];
        path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, format("%s.txt", filename));
        log_debug("Checking modification times: raw='%s' vs txt='%s'", buf, txt_path);
        err = check_modification_date_sdl(buf, txt_path);
        if (err)
        {
            /* Text file is newer - close raw and regenerate */
            log_info("Text file '%s.txt' is newer than raw file - regenerating", filename);
            ang_file_close(fd);
            fd = NULL;
        }
        else
        {
            log_debug("Raw file '%s.raw' is up to date", filename);
        }
#endif /* CHECK_MODIFICATION_TIME */

        /* Attempt to parse the "raw" file */
        if (fd && !err)
            err = init_info_raw(fd, head);

        /* Close it */
        ang_file_close(fd);
    }

    /* Do we have to parse the *.txt file? */
    if (err)
    {
        /*** Make the fake arrays ***/

        /* Allocate the "*_info" array */
        head->info_ptr = mem_alloc_array(head->info_size, char);

        /* MegaHack -- make "fake" arrays */
        if (z_info)
        {
            head->name_ptr = mem_alloc_array(z_info->fake_name_size, char);
            head->text_ptr = mem_alloc_array(z_info->fake_text_size, char);
        }

        /*** Load the ascii template file ***/

        /* Build the filename */
        path_build(buf, sizeof(buf), ANGBAND_DIR_EDIT, format("%s.txt", filename));

        /* Open the file */
        fp = ang_file_open(buf, "r");

        /* Parse it */
        if (!fp)
            quit(format("Cannot open '%s.txt' file.", filename));

        /* Parse the file */
        err = init_info_txt(fp, buf, head, head->parse_info_txt);

        /* Close it */
        ang_file_close(fp);

        /* Errors */
        if (err)
            display_parse_error(filename, err, buf);

        /*** Dump the binary image file ***/

        /* File type is "DATA" */
        FILE_TYPE(FILE_TYPE_DATA);

        /* Build the filename */
        path_build(buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the file */
        fd = ang_file_open(buf, "rb");

        /* Failure */
        if (!fd)
        {
            int mode = 0644;

            /* Grab permissions */
            safe_setuid_grab();

            /* Create a new file */
            fd = sdl_fmake(buf, mode);

            /* Drop permissions */
            safe_setuid_drop();

            /* Failure */
            if (!fd)
            {
                /* Complain */
                plog(format("Cannot create the '%s' file!", buf));

                /* Continue */
                return (0);
            }
        }

        /* Close it */
        ang_file_close(fd);

        /* Grab permissions */
        safe_setuid_grab();

        /* Attempt to create the raw file */
        fd = ang_file_open(buf, "wb");

        /* Drop permissions */
        safe_setuid_drop();

        /* Failure */
        if (!fd)
        {
            /* Complain */
            plog(format("Cannot write the '%s' file!", buf));

            /* Continue */
            return (0);
        }

        /* Dump to the file */
        if (fd)
        {
            /* Dump it */
            sdl_write(fd, (cptr)head, head->head_size);

            /* Dump the "*_info" array */
            sdl_write(fd, head->info_ptr, head->info_size);

            /* Dump the "*_name" array */
            sdl_write(fd, head->name_ptr, head->name_size);

            /* Dump the "*_text" array */
            sdl_write(fd, head->text_ptr, head->text_size);

            /* Close */
            ang_file_close(fd);
        }

        /*** Kill the fake arrays ***/

        /* Free the "*_info" array */
        mem_free_null(head->info_ptr);

        /* MegaHack -- Free the "fake" arrays */
        if (z_info)
        {
            mem_free_null(head->name_ptr);
            mem_free_null(head->text_ptr);
        }

#endif /* ALLOW_TEMPLATES */

        /*** Load the binary image file ***/

        /* Build the filename */
        path_build(buf, sizeof(buf), ANGBAND_DIR_DATA, format("%s.raw", filename));

        /* Attempt to open the "raw" file */
        fd = ang_file_open(buf, "rb");

        /* Process existing "raw" file */
        if (!fd)
            quit(format("Cannot load '%s.raw' file.", filename));

        /* Attempt to parse the "raw" file */
        err = init_info_raw(fd, head);

        /* Close it */
        ang_file_close(fd);

        /* Error */
        if (err)
            quit(format("Cannot parse '%s.raw' file.", filename));

#ifdef ALLOW_TEMPLATES
    }
#endif /* ALLOW_TEMPLATES */

    /* Success */
    return (0);
}

/*
 * Free the allocated memory for the info-, name-, and text- arrays.
 */
errr free_info(header* head)
{
    if (head->info_size)
        mem_free_null(head->info_ptr);

    if (head->name_size)
        mem_free_null(head->name_ptr);

    if (head->text_size)
        mem_free_null(head->text_ptr);

    /* Success */
    return (0);
}

/*
 * Initialize the "z_info" array
 */
errr init_z_info(void)
{
    errr err;

    /* Init the header */
    init_header(&z_head, 1, sizeof(maxima));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    z_head.parse_info_txt = parse_z_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("limits", &z_head);

    /* Set the global variables */
    z_info = z_head.info_ptr;

    return (err);
}

/*
 * Initialize the "f_info" array
 */
errr init_f_info(void)
{
    errr err;

    /* Init the header */
    init_header(&f_head, z_info->f_max, sizeof(feature_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    f_head.parse_info_txt = parse_f_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("terrain", &f_head);

    /* Set the global variables */
    f_info = f_head.info_ptr;
    f_name = f_head.name_ptr;
    f_text = f_head.text_ptr;

    return (err);
}

/*
 * Initialize the "style_info" array
 */
errr init_style_info(void)
{
    errr err;
    /* Default to zero if not specified yet; will be set by limits.txt */
    init_header(&style_head, z_info->style_max, sizeof(style_type));
    style_head.parse_info_txt = parse_style_info;
    err = init_info("style", &style_head);
    if (err)
        return err;
    /* Ensure M: banner strings are loaded even if RAW cache was used. */
    styles_reload_messages_from_text();
    /* Load level/vault rules from separate file (always parse text for side-effects). */
    {
        ang_file* fp;
        char buf[1024];
        header levels_head;
        init_header(&levels_head, 1, 1);
        path_build(buf, sizeof(buf), ANGBAND_DIR_EDIT,
            format("%s.txt", "style-levels"));
        fp = ang_file_open(buf, "r");
        if (!fp)
            quit("Cannot open 'style-levels.txt' file.");
        {
            char linebuf[1024];
            err = init_info_txt(fp, linebuf, &levels_head, parse_style_levels);
        }
        ang_file_close(fp);
        if (err)
        {
            display_parse_error("style-levels", err, "style-levels");
            return err;
        }
    }

    return 0;
}

errr init_partition_info(void)
{
    errr err;
    ang_file* fp;
    char path[1024];
    char linebuf[1024];
    header part_head;

    init_header(&part_head, 1, 1);
    partition_config_reset();

    path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "partition"));
    fp = ang_file_open(path, "r");
    if (!fp)
        quit("Cannot open 'partition.txt' file.");

    err = init_info_txt(fp, linebuf, &part_head, parse_partition_info);
    ang_file_close(fp);

    if (err)
    {
        display_parse_error("partition", err, linebuf);
        return err;
    }

    return 0;
}

/*
 * Initialize the "k_info" array
 */
errr init_k_info(void)
{
    errr err;

    /* Init the header */
    init_header(&k_head, z_info->k_max, sizeof(object_kind));

#ifdef ALLOW_TEMPLATES
    k_head.parse_info_txt = parse_k_info;
#endif

    err = init_info("object", &k_head);

    k_info = k_head.info_ptr;
    k_name = k_head.name_ptr;
    k_text = k_head.text_ptr;

    return (err);
}

errr init_b_info(void)
{
    errr err;

    init_header(&b_head, z_info->b_max, sizeof(ability_type));

#ifdef ALLOW_TEMPLATES
    b_head.parse_info_txt = parse_b_info;
#endif

    err = init_info("ability", &b_head);

    b_info = b_head.info_ptr;
    b_name = b_head.name_ptr;
    b_text = b_head.text_ptr;

    return (err);
}

errr init_a_info(void)
{
    errr err;

    init_header(&a_head, z_info->art_max, sizeof(artefact_type));

#ifdef ALLOW_TEMPLATES
    a_head.parse_info_txt = parse_a_info;
#endif

    err = init_info("artefact", &a_head);

    a_info = a_head.info_ptr;
    a_text = a_head.text_ptr;

    return (err);
}

void ensure_artifact_guids(void)
{
    if (!a_info || !z_info)
        return;

    for (int i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        if (!a_ptr)
            continue;

        if (!score_guid_is_zero(&a_ptr->guid))
            continue;

        const char* name = a_ptr->name[0] ? a_ptr->name : "unknown-artifact";
        a_ptr->guid = score_guid_from_string(name, (u32b)i);
    }
}

void ensure_artifact_spawn_numbers(void)
{
    if (!a_info || !z_info)
        return;

    for (int i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        if (!a_ptr)
            continue;
        if (a_ptr->spawn_num == 0)
            a_ptr->spawn_num = 1;
    }
}

errr init_e_info(void)
{
    errr err;

    init_header(&e_head, z_info->e_max, sizeof(ego_item_type));

#ifdef ALLOW_TEMPLATES
    e_head.parse_info_txt = parse_e_info;
#endif

    err = init_info("special", &e_head);

    e_info = e_head.info_ptr;
    e_name = e_head.name_ptr;
    e_text = e_head.text_ptr;

    return (err);
}

errr init_r_info(void)
{
    errr err;

    init_header(&r_head, z_info->r_max, sizeof(monster_race));

#ifdef ALLOW_TEMPLATES
    r_head.parse_info_txt = parse_r_info;
#endif

    err = init_info("monster", &r_head);

    r_info = r_head.info_ptr;
    r_name = r_head.name_ptr;
    r_text = r_head.text_ptr;

    return (err);
}

errr init_v_info(void)
{
    errr err;

    init_header(&v_head, z_info->v_max, sizeof(vault_type));

#ifdef ALLOW_TEMPLATES
    v_head.parse_info_txt = parse_v_info;
#endif

    err = init_info("vault", &v_head);

    v_info = v_head.info_ptr;
    v_name = v_head.name_ptr;
    v_text = v_head.text_ptr;

    return (err);
}

errr init_rt_info(void)
{
    errr err;
    init_header(&rt_head, z_info->rt_max, sizeof(runtype_type));
#ifdef ALLOW_TEMPLATES
    rt_head.parse_info_txt = parse_rt_info;
#endif
    err = init_info("runtypes", &rt_head);

    runtype_info = rt_head.info_ptr;
    return err;
}

errr init_p_info(void)
{
    errr err;

    init_header(&p_head, z_info->p_max, sizeof(player_race));

#ifdef ALLOW_TEMPLATES
    p_head.parse_info_txt = parse_p_info;
#endif

    err = init_info("race", &p_head);

    p_info = p_head.info_ptr;
    p_name = p_head.name_ptr;
    p_text = p_head.text_ptr;

    return (err);
}

errr init_c_info(void)
{
    errr err;

    init_header(&c_head, z_info->c_max, sizeof(character_profile));

#ifdef ALLOW_TEMPLATES
    c_head.parse_info_txt = parse_c_info;
#endif

    err = init_info("character", &c_head);

    c_info = c_head.info_ptr;
    c_name = c_head.name_ptr;
    c_text = c_head.text_ptr;

    return (err);
}

errr init_h_info(void)
{
    errr err;

    init_header(&h_head, z_info->h_max, sizeof(hist_type));

#ifdef ALLOW_TEMPLATES
    h_head.parse_info_txt = parse_h_info;
#endif

    err = init_info("history", &h_head);

    h_info = h_head.info_ptr;
    h_text = h_head.text_ptr;

    return (err);
}

errr init_st_info(void)
{
    errr err;

    init_header(&st_head, z_info->st_max, sizeof(story_type));

#ifdef ALLOW_TEMPLATES
    st_head.parse_info_txt = parse_st_info;
#endif

    err = init_info("story", &st_head);

    st_info = st_head.info_ptr;
    st_text = st_head.text_ptr;
    st_name = st_head.name_ptr;

    return (err);
}

errr init_cu_info(void)
{
    errr err;

    init_header(&cu_head, z_info->cu_max, sizeof(curse_type));

#ifdef ALLOW_TEMPLATES
    cu_head.parse_info_txt = parse_cu_info;
#endif

    err = init_info("curses", &cu_head);

    cu_info = cu_head.info_ptr;
    cu_text = cu_head.text_ptr;
    cu_name = cu_head.name_ptr;

    return (err);
}

errr init_mb_info(void)
{
    errr err;

    init_header(&mb_head, z_info->mb_max, sizeof(major_blessing_type));

#ifdef ALLOW_TEMPLATES
    mb_head.parse_info_txt = parse_mb_info;
#endif

    err = init_info("blessing", &mb_head);

    mb_info = mb_head.info_ptr;
    mb_text = mb_head.text_ptr;
    mb_name = mb_head.name_ptr;

    return err;
}

errr init_n_info(void)
{
    errr err;

    init_header(&n_head, 1, sizeof(names_type));

#ifdef ALLOW_TEMPLATES
    n_head.parse_info_txt = parse_n_info;
#endif

    err = init_info("names", &n_head);

    n_info = n_head.info_ptr;

    return (err);
}

errr init_flavor_info(void)
{
    errr err;

    init_header(&flavor_head, z_info->flavor_max, sizeof(flavor_type));

#ifdef ALLOW_TEMPLATES
    flavor_head.parse_info_txt = parse_flavor_info;
#endif

    err = init_info("flavor", &flavor_head);

    flavor_info = flavor_head.info_ptr;
    flavor_name = flavor_head.name_ptr;
    flavor_text = flavor_head.text_ptr;

    return (err);
}

/*
 * Initialize the special effect graphics (misc_to_attr, misc_to_char)
 */
static header effect_head;

static void effect_visuals_apply(bool ascii_mode)
{
    const effect_glyph* glyphs = (const effect_glyph*)effect_head.info_ptr;

    if (!glyphs)
        return;

    for (int i = 0; i < 256; i++)
    {
        byte attr = ascii_mode ? glyphs[i].d_attr : glyphs[i].x_attr;
        byte ch = ascii_mode ? glyphs[i].d_char : glyphs[i].x_char;

        if (ascii_mode && attr == 0 && ch == 0)
        {
            attr = (byte)(i & 0x0F);

            if (i >= 0x30 && i <= 0x3F)
                ch = (byte)'*';
            else if (i >= 0x40 && i <= 0x4F)
                ch = (byte)'|';
            else if (i >= 0x50 && i <= 0x5F)
                ch = (byte)'-';
            else if (i >= 0x60 && i <= 0x6F)
                ch = (byte)'/';
            else if (i >= 0x70 && i <= 0x7F)
                ch = (byte)'\\';
            else if (i >= 0x00 && i <= 0x09)
            {
                attr = TERM_WHITE;
                ch = (byte)('0' + i);
            }
            else if (i == ICON_UNKNOWN_ENEMY)
            {
                attr = TERM_WHITE;
                ch = (byte)'?';
            }
            else if (i == ICON_ALERT)
            {
                attr = TERM_L_RED;
                ch = (byte)'!';
            }
            else if (i == ICON_GLOW)
            {
                attr = TERM_YELLOW;
                ch = (byte)'*';
            }
            else if (i == ICON_MONSTER_SEES_PLAYER)
            {
                attr = TERM_L_BLUE;
                ch = (byte)'!';
            }
            else if (i == ICON_SLEEPING)
            {
                attr = TERM_SLATE;
                ch = (byte)'z';
            }
            else
            {
                attr = TERM_WHITE;
                ch = (byte)'*';
            }
        }

        misc_to_attr[i] = attr;
        misc_to_char[i] = (char)ch;
    }
}

void refresh_effect_visuals_for_graphics_mode(void)
{
    effect_visuals_apply(graphics_are_ascii());
}

errr init_effect_info(void)
{
    errr err;

    init_header(&effect_head, 256, sizeof(effect_glyph));

#ifdef ALLOW_TEMPLATES
    effect_head.parse_info_txt = parse_effect_info;
#endif

    err = init_info("effect", &effect_head);

    if (!err)
        refresh_effect_visuals_for_graphics_mode();

    return (err);
}

errr init_skeleton_note_info(void)
{
    errr err;

    if (z_info && z_info->skeleton_note_max <= 0)
    {
        log_warn("skeleton_note_max not set in limits.txt (or 0), defaulting to 420");
        z_info->skeleton_note_max = 420;
    }
    else
    {
        log_debug("skeleton_note_max initialized to %d", z_info->skeleton_note_max);
    }

    init_header(&skeleton_note_head, z_info->skeleton_note_max,
        sizeof(skeleton_note_template));

#ifdef ALLOW_TEMPLATES
    skeleton_note_head.parse_info_txt = parse_skeleton_note_info;
#endif

    err = init_info("skeleton_note", &skeleton_note_head);

    skeleton_note_info = (skeleton_note_template*)skeleton_note_head.info_ptr;
    skeleton_note_text = skeleton_note_head.text_ptr;

    return (err);
}

errr init_quest_info(void)
{
    errr err;

    init_header(&quest_head, z_info->quest_max, sizeof(quest_type));

#ifdef ALLOW_TEMPLATES
    quest_head.parse_info_txt = parse_quest_info;
#endif

    err = init_info("quest", &quest_head);

    quest_info = quest_head.info_ptr;
    quest_name_text = quest_head.name_ptr;
    quest_desc_text = quest_head.text_ptr;
    q_text = quest_head.text_ptr;

    return (err);
}

errr init_oath_info(void)
{
    errr err;

    init_header(&oath_head, z_info->oath_max, sizeof(oath_type));

#ifdef ALLOW_TEMPLATES
    oath_head.parse_info_txt = parse_oath_info;
#endif

    err = init_info("oath", &oath_head);

    oath_info = oath_head.info_ptr;
    oath_name_text = oath_head.name_ptr;
    oath_desc_text = oath_head.text_ptr;

    return (err);
}
