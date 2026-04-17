/* File: runtime/runtime-dungeon-presentation.c */

#include "angband.h"
#include "app/app-session.h"
#include "app/app-ui.h"
#include "blitz.h"
#include "log/log.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
#include "runtime/runtime-dungeon-internal.h"
#include "runtime/runtime-dungeon.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-semantic-scene.h"

#include <string.h>

static char g_active_partition_banner_text[1024] = "";
static u64b g_active_partition_banner_started_ms = 0;
static u32b g_active_partition_banner_hold_ms = 0;

static int last_partition_pi = -1;
static level_partition_kind last_partition_kind = LEVEL_PART_NONE;

/* Track which partitions have been narrated and the last narrated style. */
static u32b partition_narrated_mask = 0;
static int last_narrated_style_idx = -1;

/* Forward declarations for partition kind helpers (defined later in file). */
static bool is_big_partition_kind(level_partition_kind kind);
static bool is_small_cave_partition_kind(level_partition_kind kind);

enum {
    DUNGEON_NARRATIVE_BANNER_POP_IN_MS = 220u,
    DUNGEON_NARRATIVE_BANNER_POP_OUT_MS = 260u
};
void runtime_dungeon_reset_level_entry_tracking(void)
{
    g_labyrinth_view_active = false;
    g_active_partition_banner_text[0] = '\0';
    g_active_partition_banner_started_ms = 0;
    g_active_partition_banner_hold_ms = 0;
    last_partition_pi = -1;
    last_partition_kind = LEVEL_PART_NONE;
    partition_narrated_mask = 0;
    last_narrated_style_idx = -1;
}

static void dungeon_refresh_partition_banner_snapshot(void);

static byte narrative_banner_seconds(void)
{
    byte seconds;

    if (!op_ptr)
        return NARRATIVE_BANNER_SECONDS_DEFAULT;

    seconds = op_ptr->narrative_banner_seconds;
    if (seconds > NARRATIVE_BANNER_SECONDS_MAX)
        return NARRATIVE_BANNER_SECONDS_DEFAULT;

    return seconds;
}

static u32b narrative_banner_total_ms(void)
{
    return g_active_partition_banner_hold_ms
        + DUNGEON_NARRATIVE_BANNER_POP_IN_MS
        + DUNGEON_NARRATIVE_BANNER_POP_OUT_MS;
}

static void dungeon_refresh_partition_banner_snapshot(void)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!session)
        return;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return;

    app_session_mark_snapshot_dirty(session, APP_SNAPSHOT_INVALIDATE_ALL);
    (void)app_session_build_dungeon_snapshot(session, 0, 0, 0);
}

void clear_active_narrative_banner(void)
{
    bool had_active = g_active_partition_banner_text[0]
        || g_active_partition_banner_started_ms != 0
        || g_active_partition_banner_hold_ms != 0;

    g_active_partition_banner_text[0] = '\0';
    g_active_partition_banner_started_ms = 0;
    g_active_partition_banner_hold_ms = 0;

    if (had_active)
        dungeon_refresh_partition_banner_snapshot();
}

bool dungeon_active_narrative_banner_animating(u64b now_ms)
{
    if (!g_active_partition_banner_text[0])
        return false;

    if (narrative_banner_seconds() == 0
        || now_ms >= g_active_partition_banner_started_ms
            + narrative_banner_total_ms())
    {
        clear_active_narrative_banner();
        return false;
    }

    return true;
}

bool dungeon_query_active_narrative_banner(u64b now_ms, char* text,
    size_t text_size, u64b* started_ms, u32b* hold_ms)
{
    if (!dungeon_active_narrative_banner_animating(now_ms))
        return false;

    if (text && text_size)
        SDL_strlcpy(text, g_active_partition_banner_text, text_size);
    if (started_ms)
        *started_ms = g_active_partition_banner_started_ms;
    if (hold_ms)
        *hold_ms = g_active_partition_banner_hold_ms;

    return true;
}

/*
 * Transition templates for partition narrative.
 * Each template takes (old_S, new_S) as %s arguments.
 */
static const char* transition_templates[] = {
    "The %s gives way to %s.",
    "You leave the %s behind; ahead lies %s.",
    "The %s fades. Now %s surrounds you.",
    "Gone is the %s. In its place, %s.",
    "The %s recedes as %s closes around you.",
};
#define NUM_TRANSITION_TEMPLATES 5

static const char* partition_structural_text(level_partition_kind kind)
{
    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return "The passage splits and twists into a dark labyrinth.";
    case LEVEL_PART_CHASM:
        return "A vast darkness yawns below; only narrow bridges span the gulf.";
    case LEVEL_PART_BIG_CAVE:
        return "A great cavern opens before you, its roof lost in shadow.";
    default:
        return NULL;
    }
}

static const char* big_cave_elemental_text(void)
{
    big_cave_type_t cave_type =
        level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px);
    switch (cave_type)
    {
    case BIG_CAVE_FIRE:
        return "Searing heat closes around you, and you feel your strength waning.";
    case BIG_CAVE_ICE:
        return "Bitter cold gnaws at your bones, and you shiver with a deathly chill.";
    case BIG_CAVE_POIS:
        return "A noxious miasma fills the air, and poison seeps into your lungs.";
    default:
        return "You feel exposed and vulnerable in this vast empty space.";
    }
}

static void append_narrative_piece(char* buf, size_t size, const char* text)
{
    if (!text || !text[0])
        return;

    if (buf[0])
        SDL_strlcat(buf, " ", size);
    SDL_strlcat(buf, text, size);
}

static void build_partition_narrative_text(int old_sidx, int new_sidx,
    level_partition_kind kind, char* buf, size_t size)
{
    const char* structural = partition_structural_text(kind);
    bool is_transition;

    if (!buf || size == 0)
        return;
    buf[0] = '\0';

    if (structural)
    {
        append_narrative_piece(buf, size, structural);
        if (kind == LEVEL_PART_BIG_CAVE)
        {
            const char* elem = big_cave_elemental_text();
            if (elem)
                append_narrative_piece(buf, size, elem);
        }
    }

    if (is_small_cave_partition_kind(kind))
    {
        append_narrative_piece(buf, size,
            "The air grows close and frowsty in a cramped cave.");
    }

    is_transition = (old_sidx >= 0 && old_sidx != new_sidx);
    if (is_transition)
    {
        const char* old_s = styles_get_style_short_desc(old_sidx);
        const char* new_s = styles_get_style_short_desc(new_sidx);
        if (old_s && new_s)
        {
            char transition_buf[256];
            int tmpl = rand_int(NUM_TRANSITION_TEMPLATES);
            strnfmt(transition_buf, sizeof(transition_buf),
                transition_templates[tmpl], old_s, new_s);
            append_narrative_piece(buf, size, transition_buf);
        }
        else
        {
            const char* m1 = styles_get_style_m1(new_sidx);
            append_narrative_piece(buf, size, m1);
        }
    }
    else
    {
        const char* m1 = styles_get_style_m1(new_sidx);
        append_narrative_piece(buf, size, m1);
    }

    append_narrative_piece(buf, size, styles_get_style_m2(new_sidx));
}

static bool narrative_banner_popup_enabled(void)
{
    return narrative_banner_seconds() > 0;
}

static void display_narrative_text(cptr text)
{
    if (!text || !text[0] || !narrative_banner_popup_enabled())
        return;

    SDL_strlcpy(g_active_partition_banner_text, text,
        sizeof(g_active_partition_banner_text));
    g_active_partition_banner_started_ms = platform_monotonic_ms();
    g_active_partition_banner_hold_ms
        = (u32b)narrative_banner_seconds() * 1000u;
    dungeon_refresh_partition_banner_snapshot();
}

static void display_partition_narrative_banner(int old_sidx, int new_sidx,
    level_partition_kind kind)
{
    char buf[1024];

    build_partition_narrative_text(old_sidx, new_sidx, kind, buf, sizeof(buf));
    display_narrative_text(buf);
}

static bool dungeon_fullscreen_scene_enter(ui_information_scene_scope* scope,
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

static bool dungeon_fullscreen_scene_present(const app_ui_scene* scene)
{
    return scene ? ui_information_scene_present_ui(scene) : false;
}

static app_ui_panel* dungeon_fullscreen_scene_begin(app_ui_scene* scene,
    bool overlay_dungeon)
{
    return ui_semantic_scene_begin_plain(scene,
        overlay_dungeon ? APP_UI_SCENE_FLAG_USE_BACKDROP : 0,
        overlay_dungeon ? APP_UI_LAYER_TRANSIENT : APP_UI_LAYER_MODAL,
        0, NULL, 0, NULL, TERM_SLATE,
        overlay_dungeon ? 980 : 900,
        overlay_dungeon ? 1700 : 1500);
}

static bool dungeon_fullscreen_add_paragraph(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, byte story, cptr text)
{
    if (!scene || !panel)
        return false;
    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;

    return app_ui_panel_add_rich_text_ex(scene, panel, attr, story,
        (text && text[0]) ? text : " ");
}

static bool dungeon_fullscreen_add_body_hint(app_ui_panel* panel, byte attr,
    cptr text)
{
    if (!panel)
        return false;

    return app_ui_panel_add_body_line(panel, attr,
        (text && text[0]) ? text : " ");
}

static bool confirm_enter_morgoth_hall_build_ui_scene(app_ui_scene* scene,
    bool overlay_dungeon, bool steamdeck)
{
    app_ui_panel* panel;

    panel = dungeon_fullscreen_scene_begin(scene, overlay_dungeon);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_RED, "The Iron Gates of Angband");

    if (!dungeon_fullscreen_add_paragraph(scene, panel, TERM_WHITE, 0,
            "Beyond this passage lies the black hall of Morgoth Bauglir, "
            "the Dark Enemy, and the last of the Iron Hells."))
    {
        return false;
    }
    if (!dungeon_fullscreen_add_paragraph(scene, panel, TERM_L_RED, 0,
            "If you pass within, you may not return until you bear a "
            "Silmaril. Steel yourself: to enter is to choose doom or glory."))
    {
        return false;
    }

    return dungeon_fullscreen_add_body_hint(panel, TERM_YELLOW,
        steamdeck ? "Enter Morgoth's hall? [y/n/space]"
                  : "Enter Morgoth's hall? [y/n]");
}

static bool blitz_unlock_build_ui_scene(app_ui_scene* scene, bool overlay_dungeon)
{
    static const char* paragraphs[] = {
        "Blitz is a self-contained challenge run.",
        "Story progress, metaruns, saves, and score stay separate.",
        "Each Blitz run lets you choose character flow, oaths, blessings, and curses.",
        "Run history entries are still recorded and marked as Blitz.",
        NULL,
    };
    app_ui_panel* panel;

    panel = dungeon_fullscreen_scene_begin(scene, overlay_dungeon);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_YELLOW,
        "Congratulations, you have unlocked Blitz Mode!");

    for (int i = 0; paragraphs[i]; i++)
    {
        if (!dungeon_fullscreen_add_paragraph(scene, panel,
                (i == 0) ? TERM_L_WHITE : (i == 3) ? TERM_SLATE : TERM_WHITE,
                0, paragraphs[i]))
        {
            return false;
        }
    }

    return dungeon_fullscreen_add_body_hint(panel, TERM_L_BLUE,
        "Press any key to continue.");
}

bool runtime_dungeon_confirm_enter_morgoth_hall(void)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    app_session* session = app_session_current();
    char ch;
    bool overlay_dungeon = false;
    bool steamdeck = steamdeck_controls_active();

    message_flush();

    if (!dungeon_fullscreen_scene_enter(&scope, &overlay_dungeon))
    {
        log_error("morgoth hall confirm: semantic scene unavailable");
        quit("Morgoth hall confirmation requires the snapshot UI renderer.");
        return false;
    }

    if (!confirm_enter_morgoth_hall_build_ui_scene(&scene, overlay_dungeon,
            steamdeck)
        || !dungeon_fullscreen_scene_present(&scene))
    {
        ui_information_scene_leave(&scope);
        log_error("morgoth hall confirm: semantic scene presentation failed");
        quit("Morgoth hall confirmation could not be displayed.");
        return false;
    }

    while (true)
    {
        ch = (char)ui_information_scene_wait_key_nonrepeat();
        if (quick_messages)
            break;
        if (ch == ESCAPE)
            break;
        if (strchr("YyNn", ch) || (steamdeck && ch == ' '))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }

    if (session)
        app_session_clear_inputs(session);
    ui_information_scene_leave(&scope);
    return ((ch == 'Y') || (ch == 'y') || (steamdeck && ch == ' '));
}


void runtime_dungeon_update_labyrinth_view_state(bool handle_now)
{
    if (!p_ptr || p_ptr->is_dead)
        return;

    level_partition_kind kind = level_partition_kind_for_point(p_ptr->py, p_ptr->px);
    bool want = (kind == LEVEL_PART_LABYRINTH);

    if (want == g_labyrinth_view_active)
        return;

    g_labyrinth_view_active = want;

    p_ptr->redraw |= (PR_MAP);
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS | PU_DISTANCE);

    if (handle_now)
        handle_stuff();
}

static bool is_big_partition_kind(level_partition_kind kind)
{
    return (kind == LEVEL_PART_LABYRINTH || kind == LEVEL_PART_BIG_CAVE
        || kind == LEVEL_PART_CHASM);
}

static bool is_small_cave_partition_kind(level_partition_kind kind)
{
    return (kind == LEVEL_PART_CAVEY);
}

static byte partition_discovery_lore_flag(level_partition_kind kind)
{
    if (!p_ptr)
        return 0;

    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return DISC_LORE_LABYRINTH;
    case LEVEL_PART_CHASM:
        return DISC_LORE_CHASM;
    case LEVEL_PART_BIG_CAVE:
    {
        switch (level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px))
        {
        case BIG_CAVE_ICE:
            return DISC_LORE_BIG_CAVE_ICE;
        case BIG_CAVE_FIRE:
            return DISC_LORE_BIG_CAVE_FIRE;
        case BIG_CAVE_POIS:
            return DISC_LORE_BIG_CAVE_POIS;
        default:
            return 0;
        }
    }
    default:
        return 0;
    }
}

static cptr partition_discovery_lore_text(level_partition_kind kind)
{
    big_cave_type_t cave_type = BIG_CAVE_NONE;

    if (kind == LEVEL_PART_BIG_CAVE && p_ptr)
        cave_type = level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px);

    return partition_config_get_discovery_text(kind, cave_type);
}

static void maybe_award_partition_discovery_xp(level_partition_kind kind)
{
    byte bit = partition_discovery_lore_flag(kind);
    cptr text = partition_discovery_lore_text(kind);

    if (!bit || !text)
        return;

    if (p_ptr->discovery_lore_flags & bit)
        return;

    p_ptr->discovery_lore_flags |= bit;
    gain_exp(300);
    display_narrative_text(text);
}

static cptr vault_entry_message_for_name(cptr vault_name)
{
    int i;

    if (!vault_name || !vault_name[0])
        return NULL;

    for (i = 0; i < z_info->v_max; i++)
    {
        vault_type* v_ptr = &v_info[i];
        cptr name;

        if (!v_ptr->name)
            continue;

        name = v_name + v_ptr->name;
        if (strcmp(name, vault_name) != 0)
            continue;

        if (!v_ptr->message)
            return NULL;

        return v_text + v_ptr->message;
    }

    return NULL;
}

void runtime_dungeon_describe_greater_vault_entry(cptr vault_name)
{
    cptr text = vault_entry_message_for_name(vault_name);

    if (!text)
        return;

    /* Great vault entries should always land in the message log too, even
     * when the current setting also shows them as banners. */
    msg_print(text);

    display_narrative_text(text);
}

void runtime_dungeon_handle_partition_entry(bool force_message)
{
    if (!p_ptr || p_ptr->is_dead)
        return;

    int pi = level_partition_index_for_point(p_ptr->py, p_ptr->px);
    level_partition_kind kind = level_partition_kind_for_point(p_ptr->py, p_ptr->px);
    int sidx = styles_decode_color_style(cave_color[p_ptr->py][p_ptr->px]);

    bool is_big = is_big_partition_kind(kind);
    bool was_big = is_big_partition_kind(last_partition_kind);
    bool entered_big = false;
    if (force_message)
    {
        entered_big = is_big;
    }
    else if (is_big)
    {
        if (!was_big)
            entered_big = true;
        else if (pi != last_partition_pi || kind != last_partition_kind)
            entered_big = true;
    }

    if (entered_big)
        maybe_award_partition_discovery_xp(kind);

    if (!force_message && (pi >= 0) && (pi < 25) && (sidx >= 0))
    {
        u32b bit = (u32b)(1U << pi);
        if (!(partition_narrated_mask & bit))
        {
            if (narrative_banner_popup_enabled())
                display_partition_narrative_banner(
                    last_narrated_style_idx, sidx, kind);

            if (is_small_cave_partition_kind(kind))
                msg_print("Here torch and lamp drink their fuel twice as fast.");

            partition_narrated_mask |= bit;
            last_narrated_style_idx = sidx;
        }
    }

    last_partition_pi = pi;
    last_partition_kind = kind;
}


void runtime_dungeon_show_initial_partition_banner(void)
{
    if (narrative_banner_popup_enabled())
    {
        int spawn_pi = level_partition_index_for_point(p_ptr->py, p_ptr->px);
        int spawn_sidx = styles_decode_color_style(cave_color[p_ptr->py][p_ptr->px]);
        level_partition_kind spawn_kind =
            level_partition_kind_for_point(p_ptr->py, p_ptr->px);
        if (spawn_sidx >= 0) {
            display_partition_narrative_banner(-1, spawn_sidx, spawn_kind);
            if (spawn_pi >= 0 && spawn_pi < 25)
                partition_narrated_mask |= (u32b)(1U << spawn_pi);
            last_narrated_style_idx = spawn_sidx;
        }
    }
}
void runtime_dungeon_prepare_death_knowledge(void)
{
    int i;

    object_type* o_ptr;

    /* Hack -- Know everything in the inven/equip */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Aware and Known */
        object_aware(o_ptr);
        object_known(o_ptr);
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Hack -- Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();
}

static bool story_intro_build_ui_scene(app_ui_scene* scene,
    bool overlay_dungeon, cptr* intro_texts, int total, int start_index,
    int* out_next_index)
{
    app_ui_panel* panel;
    int index = start_index;

    if (out_next_index)
        *out_next_index = start_index;
    if (!scene || !intro_texts || start_index < 0 || start_index >= total)
        return false;

    panel = dungeon_fullscreen_scene_begin(scene, overlay_dungeon);
    if (!panel)
        return false;

    if (!dungeon_fullscreen_add_paragraph(scene, panel, TERM_WHITE,
            STORY_FLAG_USE, intro_texts[index]))
    {
        return false;
    }
    index++;

    if (index >= total)
    {
        if (!dungeon_fullscreen_add_body_hint(panel, TERM_L_WHITE,
                "[c] Change difficulty (experienced players)"))
        {
            return false;
        }
        if (!dungeon_fullscreen_add_body_hint(panel, TERM_L_WHITE,
                "(press any key to finish, S skips)"))
        {
            return false;
        }
    }
    else
    {
        if (!dungeon_fullscreen_add_body_hint(panel, TERM_L_WHITE,
                "(press any key, S skips)"))
        {
            return false;
        }
    }

    if (out_next_index)
        *out_next_index = index;
    return true;
}

/**
 * Introductory narrative display, one paragraph per prompt.
 * Implemented as a static function to restrict linkage.
 */
void runtime_dungeon_print_story_intro(void)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    app_session* session = app_session_current();
    bool overlay_dungeon = false;

    /* Narrative paragraphs as valid C string literals with embedded \n */
    cptr intro_texts[] = {
        "You awaken in darkness.\n"
        "No name. No memory.\n"
        "Only a quiet ache of courage deep inside you,\n"
        "like embers buried beneath ash.\n",

        "Far below, Morgoth waits upon his throne-\n"
        "iron-dark and crowned in flame.\n"
        "Upon his brow shine three Silmarils, stolen stars.\n"
        "He senses your stirring. He knows you will come.\n",

        "Far above, beyond the shadows of Angband,\n"
        "the Valar watch silently.\n"
        "They offer no guidance, yet their presence\n"
        "fills you with strength-and dread.\n",

        "You will return many times, each death and rebirth\n"
        "etched into the endless stone halls of Mandos.\n"
        "Each fall will draw your spirit deeper into shadow,\n"
        "closer to a doom from which you cannot escape.\n",

        "Yet each victory-each Silmaril wrested from Morgoth's crown-\n"
        "will brighten the Valar's hope,\n"
        "even as your soul grows thinner,\n"
        "your strength fading with every triumph.\n",

        "You envy the Edain, whose Gift from Iluvatar\n"
        "frees them from the bonds of Mandos and the world.\n"
        "Yet you do not know if such release can ever be yours.\n"
        "You do not know who-or even what-you truly are.\n",

        "For each time you awaken,\n"
        "you will carry the names of heroes beloved and feared-\n"
        "bright spirits, fiery hearts, proud kings and exiles,\n"
        "wanderers beneath sun and stars,\n"
        "whose courage you borrow, but whose fates are not your own.\n",

        "This is the trial set by the Valar:\n"
        "to reclaim your forgotten name,\n"
        "to balance shadow and light,\n"
        "and to find within the borrowed glory of others\n"
        "your true self.\n",

        "Now the path before you opens,\n"
        "and your trial begins.\n"
    };
    int total = sizeof(intro_texts) / sizeof(intro_texts[0]);
    int index = 0;

    platform_music_play_main_full();

    if (!dungeon_fullscreen_scene_enter(&scope, &overlay_dungeon))
    {
        log_error("story intro: semantic scene unavailable");
        quit("Story intro requires the snapshot UI renderer.");
        return;
    }

    while (index < total)
    {
        int next_index = index;
        int key;
        bool final_page;

        if (!story_intro_build_ui_scene(&scene, overlay_dungeon, intro_texts,
                total, index, &next_index))
        {
            ui_information_scene_leave(&scope);
            log_error("story intro: semantic scene presentation failed");
            quit("Story intro could not be displayed.");
            return;
        }

        if (!ui_semantic_scene_present_and_wait_key(&scene, true, false,
                APP_WAIT_REASON_NONE, &key))
        {
            ui_information_scene_leave(&scope);
            log_error("story intro: semantic scene presentation failed");
            quit("Story intro could not be displayed.");
            return;
        }
        if (key == 'S')
            break;

        final_page = (next_index >= total);
        if (final_page && (key == 'c' || key == 'C'))
        {
            if (session)
                ui_semantic_scene_clear_pending_input();
            ui_information_scene_leave_without_restore(&scope);
            choose_difficulty_level();
            return;
        }

        index = next_index;
    }

    if (session)
        ui_semantic_scene_clear_pending_input();
    ui_information_scene_leave_without_restore(&scope);
}

void runtime_dungeon_maybe_show_blitz_unlock_screen(void)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    app_session* session = app_session_current();
    bool overlay_dungeon = false;

    if (run_mode_is_blitz())
        return;
    if (!op_ptr || op_ptr->opt[OPT_unlock_blitz_mode])
        return;
    if (metarun_completed_count() < 1)
        return;

    if (!dungeon_fullscreen_scene_enter(&scope, &overlay_dungeon))
    {
        log_error("blitz unlock: semantic scene unavailable");
        quit("Blitz unlock notice requires the snapshot UI renderer.");
        return;
    }

    if (!blitz_unlock_build_ui_scene(&scene, overlay_dungeon))
    {
        ui_information_scene_leave(&scope);
        log_error("blitz unlock: semantic scene presentation failed");
        quit("Blitz unlock notice could not be displayed.");
        return;
    }

    if (!ui_semantic_scene_present_and_wait_key(&scene, true, false,
            APP_WAIT_REASON_NONE, NULL))
    {
        ui_information_scene_leave(&scope);
        log_error("blitz unlock: semantic scene presentation failed");
        quit("Blitz unlock notice could not be displayed.");
        return;
    }
    if (session)
        ui_semantic_scene_clear_pending_input();
    ui_information_scene_leave_without_restore(&scope);
    op_ptr->opt[OPT_unlock_blitz_mode] = true;
    save_pane_config_to_json();
}



/*
 * Actually play a game.
 *
 * This function is called from a variety of entry points, since both
 * the standard "main.c" file, as well as several platform-specific
 * "main-xxx.c" files, call this function to start a new game with a
 * new savefile, start a new game with an existing savefile, or resume
 * a saved game with an existing savefile.
 *
 * If the "new_game" parameter is true, and the savefile contains a
 * living character, then that character will be killed, so that the
 * player may start a new game with that savefile.  This is only used
 * by the "-n" option in "main.c".
 *
 * If the savefile does not exist, cannot be loaded, or contains a dead
 * (non-wizard-mode) character, then a new game will be started.
 *
 * Some platforms (Windows) start brand new games with "savefile" and 
 * "op_ptr->base_name" both empty, and initialize them later based on 
 * the player name. To prevent weirdness, we must initialize 
 * "op_ptr->base_name" to "nameless" if it is empty.
 *
 * Note that we load the RNG state from savefiles and
 * only initialize it when starting a brand new character.
 */
