#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

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
    { "You are ready to begin a Tale with a hero of your own." }, { " " },
    { "For an easier first run, choose a hero with a high power rating" },
    { "and invest early experience in Melee and Evasion." },
    { "Read item and ability descriptions; they state exact effects and exceptions." },
    { "Use Hints & Quests for objectives and Help for the searchable Gameplay Reference." },
    { " " },
    { "In normal play you may save and quit, but saves are not checkpoints:" },
    { "death ends the run and normally removes that hero from the current Tale." },
    { "Sil-More rewards caution. You do not need to fight every enemy;" },
    { "retreat, recover, and use terrain whenever a battle turns against you." },
    { "Each death can teach you what to prepare for next time." },

    { "" }
};

const char tutorial_win_text[][100] = {
    { "You have completed the tutorial." }, { " " },
    { "You are ready to begin a Tale with a hero of your own." }, { " " },
    { "For an easier first run, choose a hero with a high power rating" },
    { "and invest early experience in Melee and Evasion." },
    { "Read item and ability descriptions; they state exact effects and exceptions." },
    { "Use Hints & Quests for objectives and Help for the searchable Gameplay Reference." },
    { " " },
    { "In normal play you may save and quit, but saves are not checkpoints:" },
    { "death ends the run and normally removes that hero from the current Tale." },
    { "Retreat, recover, and use terrain whenever a battle turns against you." },

    { "" }
};

const char tutorial_early_death_text[][100] = { { "You have been slain." },
    { " " },
    { "The run has ended. Start again when you are ready." },
    { " " },
    { "In normal play you may save and quit, but saves are not checkpoints:" },
    { "death ends the run and normally removes that hero from the current Tale." },
    { " " },
    { "When combat turns against you, retreat through controlled terrain and recover." },
    { "You can return with a better plan or leave that enemy alone." },
    { "Use Look, nearby lists, and combat history to learn what made the fight dangerous." },

    { "" } };

const char tutorial_late_death_text[][100] = {
    { "This run has ended." }, { " " },
    { "You have also met a core rule: death ends a run." },
    { "In normal play you may save and quit, but saves are not checkpoints," },
    { "and death normally removes that hero from the current Tale." },
    { " " },
    { "For an easier first run, choose a hero with a high power rating" },
    { "and invest early experience in Melee and Evasion." },
    { "Read descriptions, review Hints & Quests, and open Help when a rule is unclear." },
    { "Retreat and recover when a fight turns against you; not every foe must be killed." },
    { "Each death can teach you what to prepare for next time." },

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
    = { { "Against all hope, the Black Foe of the World is cast down," },
          { "  his form broken, his fire quenched in the wreck of his pride." },
          { "    Though malice so great may not wholly perish," },
          { "      for this age of Arda his shadow is lifted." },
          { "But Angband groans above its fallen master," },
          { "  and you are buried still beneath the roots of the North." },
          { "    The songs must be sung under open sky --" },
          { "      run now, and bear the light out of the dark!" },

          { "" } };

static bool pause_with_text_is_tutorial(const char desc[][100])
{
    return desc == tutorial_leave_text || desc == tutorial_win_text
        || desc == tutorial_early_death_text
        || desc == tutorial_late_death_text;
}

/* This renderer supports whole-line colour.  Reserve it for standalone
 * outcome headings; keep explanatory sentences neutral instead of tinting
 * unrelated words because a line happens to mention death or a skill. */
static byte pause_with_text_tutorial_attr(cptr line)
{
    if (!line)
        return TERM_WHITE;

    if (streq(line, "You have completed the tutorial."))
    {
        return TERM_L_GREEN;
    }

    if (streq(line, "You have been slain."))
    {
        return TERM_L_RED;
    }

    return TERM_WHITE;
}

static void pause_with_text_semantic_add(cptr text, byte attr,
    int base_indent, int* line_count)
{
    int leading = 0;

    if (!text)
        text = "";
    while (text[leading] == ' ')
        leading++;

    sdl_pause_text_screen_add_line(text + leading, attr,
        MAX(0, base_indent + leading));
    if (line_count)
        (*line_count)++;
}

/* Preserve source lines, colours, and relative indentation as semantic data;
 * the SDL canvas owns font sizing, wrapping, and rendering. */
static void pause_with_text_sdl(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr)
{
    int line_count = 0;
    int origin_col = col;
    int banner_col = MAX(0, col - 5);
    int tail_col = banner_col + 4;
    int n_extra = 0;
    bool tutorial_text = pause_with_text_is_tutorial(desc);

    (void)row;

    if (!sdl_pause_text_screen_begin())
    {
        log_error("Unable to open the SDL pause-text screen");
        return;
    }

    screen_save();
    Term_clear();

    if (extra)
    {
        while (extra[n_extra][0])
            n_extra++;
        origin_col = banner_col;

        if (n_extra > 0)
        {
            pause_with_text_semantic_add(extra[0], extra_attr, 0,
                &line_count);
        }
        if (n_extra > 1)
        {
            pause_with_text_semantic_add("", extra_attr, 0, &line_count);
            for (int i = 1; i < n_extra; i++)
            {
                int line_col = (i == n_extra - 1) ? tail_col : banner_col;

                pause_with_text_semantic_add(extra[i], extra_attr,
                    line_col - origin_col, &line_count);
            }
            pause_with_text_semantic_add("", TERM_WHITE, 0, &line_count);
        }
    }

    for (int i = 0; desc && desc[i][0]; i++)
    {
        byte attr = tutorial_text
            ? pause_with_text_tutorial_attr(desc[i]) : TERM_WHITE;

        pause_with_text_semantic_add(desc[i], attr, col - origin_col,
            &line_count);
    }

    sdl_pause_text_screen_set_visible_lines(0);
    Term_fresh();
    for (int i = 0; i < line_count; i++)
    {
        sdl_pause_text_screen_set_visible_lines(i + 1);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 50);
    }

    hide_cursor = true;
    (void)inkey();
    hide_cursor = false;

    sdl_pause_text_screen_hide();
    screen_load();
}

void pause_with_text(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr)
{
    pause_with_text_sdl(desc, row, col, extra, extra_attr);
}
