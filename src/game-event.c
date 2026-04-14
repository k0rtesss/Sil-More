/* File: game-event.c */

#include "angband.h"
#include "app/app-ui.h"
#include "externs.h"
#include "game-event.h"
#include "log/log.h"
#include "platform-story-font.h"
#include "ui/ui-information-scene.h"

#define PAUSE_WITH_TEXT_DOC_COLS 80
#define PAUSE_WITH_TEXT_DOC_ROWS 24

bool random_stair_location(int* sy, int* sx)
{
    int stair_y[100];
    int stair_x[100];
    int stair_num = 0;
    int y, x;

    // Note all the stairs
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_stair_bold(y, x))
            {
                stair_y[stair_num] = y;
                stair_x[stair_num] = x;
                if (stair_num < 99)
                    stair_num++;
            }
        }
    }

    // If no valid stairs are found, then bail out (paranoia)
    if (stair_num == 0)
    {
        return (false);
    }

    // Choose a random stair
    stair_num = rand_int(stair_num);
    *sy = stair_y[stair_num];
    *sx = stair_x[stair_num];

    return (true);
}

/*
 * Break the truce in Morgoth's throne room
 */
extern void break_truce(bool obvious)
{
    int i;

    monster_type* m_ptr = NULL; // default to soothe compiler warnings

    char m_name[80];

    if (p_ptr->truce)
    {
        /* Scan all other monsters */
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore dead monsters */
            if (!m_ptr->r_idx)
                continue;

            // Ignore monsters out of line of sight
            if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
                continue;

            // Ignore unalert monsters
            if (m_ptr->alertness < ALERTNESS_ALERT)
                continue;

            /* Get the monster name (using 'something' for hidden creatures) */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

            p_ptr->truce = false;
        }

        if (obvious)
            p_ptr->truce = false;

        if (!p_ptr->truce)
        {
            if (!obvious)
            {
                msg_format(
                    "%^s lets out a cry! The tension is broken.", m_name);

                /* Make a lot of noise */
                update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
                monster_perception(false, false, -10);
            }
            else
            {
                msg_print("The tension is broken.");
            }

            /* Scan all other monsters */
            for (i = mon_max - 1; i >= 1; i--)
            {
                /* Access the monster */
                m_ptr = &mon_list[i];

                /* Ignore dead monsters */
                if (!m_ptr->r_idx)
                    continue;

                /* Mark minimum desired range for recalculation */
                m_ptr->min_range = 0;
            }
        }
    }
}

const char entry_poetry[][100] = { { "Into the vast and echoing gloom," },
    { "more dread than many-tunnelled tomb" },
    //	{ "in labyrinthine pyramid" },
    //	{ "where everlasting death is hid," },
    { "  down awful corridors that wind" },
    { "    down to a menace dark enshrined;" },
    { "      down to the mountain's roots profound," },
    { "devoured, tormented, bored and ground" },
    { "by seething vermin spawned of stone;" },
    { "  down to the depths they went alone..." },

    { "" } };

const char tutorial_leave_text[][100] = {
    { "You have finished the first half of the tutorial and are ready" },
    { "to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_win_text[][100] = {
    { "Congratulations. You have survived a fire-drake (usually found" },
    { "at 900 ft!), and have finished the tutorial in fine form." },
    { "You are more than ready to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_early_death_text[][100] = { { "You have been slain." },
    { " " },
    { "A key feature of Sil (and all Roguelike games) is that you cannot" },
    { "use savepoints: if you die, that's it!" },
    { "It is thus a challenging game where you need to really *think*." },
    { " " },
    { "However, it is a bit frustrating to die before the end of the" },
    { "tutorial, so we evidentally made it a bit too deadly." }, { " " },
    { "Just restart the tutorial and you should be back to where you" },
    { "were in a couple of minutes. Remember that if combat is not going" },
    { "your way, you can try to escape and heal, then either come back" },
    { "and again to defeat your adversary, or simply ignore it." },

    { "" } };

const char tutorial_late_death_text[][100] = {
    { "Congratulations: you have finished the tutorial." }, { " " },
    { "You have also just been through a rite of passage: dying." },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },
    { " " },
    { "You are now more than ready to create a character and start playing." },
    { " " }, { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." },

    { "" }
};

const char throne_poetry[][100] = { { "Loud rose a din of laughter hoarse," },
    { "  self-loathing yet without remorse;" },
    { "    loud came a singing harsh and fierce" },
    { "      like swords of terror souls to pierce." },
    { "Red was the glare through open doors" },
    { "  of firelight mirrored on brazen floors," },
    { "    and up the arches towering clomb" },
    { "      to glooms unguessed, to vaulted dome" },
    { "        swathed in wavering smokes and steams" },
    { "          stabbed with flickering lightning-gleams." },

    { "" } };

/*
const char throne_poetry2[][100] =
{
        { "To Morgoth's hall, where dreadful feast" },
        { "he held, and drank the blood of beast" },
        { "and lives of Men, she stumbling came:" },
        { "her eyes were dazed with smoke and flame." },
        { "The pillars, reared like monstrous shores" },
        { "to bear earth's overwhelming floors," },
        { "were devil-carven, shaped with skill" },
        { "such as unholy dreams doth fill:" },
        { "they towered like trees into the air," },
        { "whose trunks are rooted in despair," },
        { "whose shade is death, whose fruit is bane," },
        { "whose boughs like serpents writhe in pain." },
        { "Beneath them ranged with spear and sword" },
        { "stood Morgoth's sable-armoured horde:" },
        { "the fire on blade and boss of shield" },
        { "was red as blood on stricken field." },
        { "Beneath a monstrous column loomed" },
        { "the throne of Morgoth, and the doomed" },
        { "and dying gasped upon the floor:" },
        { "his hideous footstool, rape of war." },

        { "" }
};
*/

const char ultimate_bug_text[][100]
    = { { "Against all hope, you defeated the Dark Enemy," },
          { "  and destroyed his physical form." },
          { "    For the rest of this age at least," },
          { "      Arda shall be free from the tyrant's shadow." },
          { "But there will be time later for reflection" },
          { "  on this great change to Arda's fate." },
          { "    You are buried still in Angband's vaults" },
          { "      -- make quick your bold escape!" },

          { "" } };

static int pause_with_text_measure_width(cptr text, int len, bool use_story,
    int cell_width)
{
    int width;

    if (!text || len <= 0)
        return 0;
    if (!use_story)
        return len * cell_width;

    width = sdl_story_font_text_width(text, len);
    if (width <= 0)
        width = len * cell_width;
    return width;
}

static int pause_with_text_space_width(bool use_story, int cell_width)
{
    int width;

    if (!use_story)
        return cell_width;

    width = sdl_story_font_text_width(" ", 1);
    if (width <= 0)
        width = cell_width;
    return width;
}

static int pause_with_text_scene_add_wrapped_segment(app_ui_scene* scene,
    app_ui_panel* panel, int row, int col, byte attr, cptr text)
{
    const char* s = text ? text : "";
    int wrap_col = PAUSE_WITH_TEXT_DOC_COLS - 2;
    int cell_width = sdl_get_cell_width();
    int wrap_pixels;
    int indent_pixels;
    int current_pixels;
    int space_pixels;
    int current_row = row;
    int rows_used = 0;
    char line[APP_UI_TEXT_MAX];
    int line_len = 0;
    bool use_story = true;

    if (!scene || !panel)
        return 0;

    if (cell_width <= 0)
        cell_width = 1;
    if (wrap_col <= col)
        wrap_col = col + 1;

    wrap_pixels = wrap_col * cell_width;
    indent_pixels = col * cell_width;
    current_pixels = indent_pixels;
    space_pixels = pause_with_text_space_width(use_story, cell_width);
    if (space_pixels <= 0)
        space_pixels = cell_width;

    if (!*s)
        return 1;

    while (*s)
    {
        if (*s == '\n')
        {
            line[line_len] = '\0';
            if (line_len > 0
                && !app_ui_panel_add_document_text_ex(scene, panel,
                    (s16b)current_row, (s16b)col, attr, STORY_FLAG_USE, line))
            {
                return 0;
            }

            rows_used++;
            current_row++;
            line_len = 0;
            current_pixels = indent_pixels;
            s++;
            continue;
        }

        while (*s == ' ')
        {
            if (line_len < (int)sizeof(line) - 1)
                line[line_len++] = ' ';
            current_pixels += space_pixels;
            s++;

            if (current_pixels >= wrap_pixels)
            {
                line[line_len] = '\0';
                if (line_len > 0
                    && !app_ui_panel_add_document_text_ex(scene, panel,
                        (s16b)current_row, (s16b)col, attr, STORY_FLAG_USE,
                        line))
                {
                    return 0;
                }

                rows_used++;
                current_row++;
                line_len = 0;
                current_pixels = indent_pixels;
            }
        }

        if (!*s)
            break;
        if (*s == '\n')
            continue;

        {
            const char* word_start = s;
            int word_chars = 0;
            int word_pixels;

            while (s[word_chars] && s[word_chars] != ' ' && s[word_chars] != '\n')
                word_chars++;
            if (word_chars == 0)
                continue;

            word_pixels = pause_with_text_measure_width(word_start, word_chars,
                use_story, cell_width);
            if (current_pixels > indent_pixels
                && (current_pixels + word_pixels) > wrap_pixels)
            {
                line[line_len] = '\0';
                if (line_len > 0
                    && !app_ui_panel_add_document_text_ex(scene, panel,
                        (s16b)current_row, (s16b)col, attr, STORY_FLAG_USE,
                        line))
                {
                    return 0;
                }

                rows_used++;
                current_row++;
                line_len = 0;
                current_pixels = indent_pixels;
            }

            for (int i = 0; i < word_chars; i++)
            {
                if (line_len < (int)sizeof(line) - 1)
                    line[line_len++] = word_start[i];
            }

            current_pixels += word_pixels;
            s += word_chars;
        }
    }

    line[line_len] = '\0';
    if (line_len > 0)
    {
        if (!app_ui_panel_add_document_text_ex(scene, panel, (s16b)current_row,
                (s16b)col, attr, STORY_FLAG_USE, line))
        {
            return 0;
        }
        rows_used++;
    }

    return rows_used ? rows_used : 1;
}

static bool pause_with_text_build_ui_scene(app_ui_scene* scene, int row, int col,
    const char desc[][100], const char extra[][100], byte extra_attr,
    bool overlay_dungeon)
{
    app_ui_panel* panel;
    int banner_lines = 0;
    int i_main = 0;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene,
        overlay_dungeon ? APP_UI_LAYER_TRANSIENT : APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;

    if (extra)
    {
        int n_extra = 0;

        banner_lines += pause_with_text_scene_add_wrapped_segment(scene, panel,
            row + banner_lines, col - 5, extra_attr, extra[0]);
        if (!banner_lines)
            return false;

        banner_lines += pause_with_text_scene_add_wrapped_segment(scene, panel,
            row + banner_lines, col - 5, extra_attr, "");
        if (banner_lines <= 0)
            return false;

        while (extra[n_extra][0])
            n_extra++;

        for (int i = 1; i < n_extra; ++i)
        {
            int shift = col - 5;
            int used;

            if (i == n_extra - 1)
                shift += 4;

            used = pause_with_text_scene_add_wrapped_segment(scene, panel,
                row + banner_lines, shift, extra_attr, extra[i]);
            if (!used)
                return false;
            banner_lines += used;
        }

        banner_lines++;
    }

    while (desc && desc[i_main][0])
    {
        int used = pause_with_text_scene_add_wrapped_segment(scene, panel,
            row + banner_lines, col, TERM_WHITE, desc[i_main]);

        if (!used)
            return false;
        banner_lines += used;
        i_main++;
    }

    if (panel->document_op_count == 0)
        return app_ui_panel_add_document_text(scene, panel, 0, 0, TERM_WHITE, " ");

    return true;
}

static bool pause_with_text_scene_enter(ui_information_scene_scope* scope,
    bool* overlay_dungeon)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (overlay_dungeon)
        *overlay_dungeon = false;
    if (!scope || !session)
        return false;
    if (!ui_information_scene_enter(scope))
        return false;

    snapshot = app_session_snapshot(session);
    if (overlay_dungeon && snapshot && snapshot->scene == APP_SCENE_KIND_DUNGEON)
        *overlay_dungeon = true;
    return true;
}

static bool pause_with_text_scene_present(ui_information_scene_scope* scope,
    const app_ui_scene* scene, bool overlay_dungeon)
{
    app_session* session = app_session_current();

    if (!scope || !scene || !session)
        return false;

    if (overlay_dungeon)
    {
        if (!app_session_publish_dungeon_overlay_scene(session, scene))
            return false;
        scope->published_overlay = true;
        (void)Term_xtra(TERM_XTRA_FRESH, 0);
        return true;
    }

    return ui_information_scene_present_ui(scene);
}

/* pause_with_text: prints name+alt, explicit blank line, then wrapped start splits */
void pause_with_text(const char desc[][100], int row, int col,
                     const char extra[][100], byte extra_attr)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    bool overlay_dungeon = false;

    if (!pause_with_text_scene_enter(&scope, &overlay_dungeon))
    {
        log_warn("pause_with_text: semantic scene unavailable");
        return;
    }

    if (!pause_with_text_build_ui_scene(&scene, row, col, desc, extra,
            extra_attr, overlay_dungeon)
        || !pause_with_text_scene_present(&scope, &scene, overlay_dungeon))
    {
        ui_information_scene_leave(&scope);
        log_warn("pause_with_text: semantic scene presentation failed");
        return;
    }

    {
        app_session* session = app_session_current();

        (void)ui_information_scene_wait_key_nonrepeat();
        if (session)
            app_session_clear_inputs(session);
    }
    ui_information_scene_leave(&scope);
}
