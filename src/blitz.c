#include "angband.h"
#include "app/app-session.h"
#include "app/app-ui.h"

#include "blitz.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "ui/ui-information-scene.h"

static run_mode g_pending_run_mode = RUN_MODE_STORY;
static run_mode g_current_run_mode = RUN_MODE_STORY;

static blitz_setup g_blitz_setup = {
    BLITZ_CHARACTER_RANDOM,
    false,
    0,
    0,
    BLITZ_EFFECT_RANDOM,
};

static int8_t g_blitz_curse_stacks[METAR_CURSE_SLOTS];
static u64b g_blitz_curses_seen = 0;
static bool g_blitz_end_summary_shown = false;

static void blitz_format_end_summary_line(byte sil_count, char* buf, size_t len)
{
    if (!buf || !len)
        return;

    if (sil_count == 1)
        SDL_strlcpy(buf, "1 Silmaril was stolen.", len);
    else
        strnfmt(buf, len, "%u Silmarils were stolen.", (unsigned)sil_count);
}

static bool blitz_build_end_summary_ui_scene(app_ui_scene* scene,
    byte sil_count)
{
    app_ui_panel* panel;
    char result_line[APP_UI_TEXT_MAX];

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 320, 560);
    app_ui_panel_set_title(panel, TERM_YELLOW, "Blitz Result");

    blitz_format_end_summary_line(sil_count, result_line, sizeof(result_line));
    if (!app_ui_panel_add_body_line(panel, TERM_L_WHITE, result_line))
        return false;

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true, "Any",
        "Continue");
}

static bool blitz_show_end_summary_ui(byte sil_count)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    app_session* session = app_session_current();

    if (!session || !ui_information_scene_enter(&scope))
        return false;

    if (!blitz_build_end_summary_ui_scene(&scene, sil_count)
        || !ui_information_scene_present_ui(&scene))
    {
        ui_information_scene_leave(&scope);
        log_warn("blitz summary: semantic scene presentation failed");
        msg_print("Blitz result summary unavailable.");
        return false;
    }

    (void)ui_information_scene_wait_key_nonrepeat();
    app_session_clear_inputs(session);
    ui_information_scene_leave(&scope);
    return true;
}

int8_t* active_curse_stacks(void)
{
    return run_mode_is_blitz() ? g_blitz_curse_stacks : metar.curse_stacks;
}

u64b* active_curses_seen_ptr(void)
{
    return run_mode_is_blitz() ? &g_blitz_curses_seen : &metar.curses_seen;
}

void run_mode_reset(void)
{
    g_pending_run_mode = RUN_MODE_STORY;
    g_current_run_mode = RUN_MODE_STORY;
    blitz_setup_reset();
    blitz_runtime_reset();
    g_blitz_end_summary_shown = false;
}

void run_mode_set_pending(run_mode mode)
{
    g_pending_run_mode = mode;
}

run_mode run_mode_pending(void)
{
    return g_pending_run_mode;
}

void run_mode_activate_pending(void)
{
    g_current_run_mode = g_pending_run_mode;
    if (g_current_run_mode != RUN_MODE_BLITZ)
        blitz_runtime_reset();
}

void run_mode_set_current(run_mode mode)
{
    g_current_run_mode = mode;
}

run_mode run_mode_current(void)
{
    return g_current_run_mode;
}

bool run_mode_is_blitz(void)
{
    return g_current_run_mode == RUN_MODE_BLITZ;
}

const char* active_score_filename(void)
{
    return run_mode_is_blitz() ? "scores-blitz.raw" : "scores.raw";
}

bool build_active_score_path(char* buf, size_t len)
{
#ifdef SIL_USE_LOCAL_DATA
    return path_build(buf, len, ANGBAND_DIR_APEX, active_score_filename());
#else
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep)
            *last_sep = '\0';
        return path_build(buf, len, meta_dir, active_score_filename());
    }

    return path_build(buf, len, ANGBAND_DIR_APEX, active_score_filename());
#endif
}

void build_active_savefile_stem(const char* base_name, char* out, size_t len)
{
    const char* stem = (base_name && base_name[0]) ? base_name : "nameless";

    if (!out || !len)
        return;

    if (run_mode_is_blitz())
        strnfmt(out, len, "blitz_%s", stem);
    else
        strnfmt(out, len, "%s", stem);
}

void blitz_setup_reset(void)
{
    g_blitz_setup.character_mode = BLITZ_CHARACTER_RANDOM;
    g_blitz_setup.oaths_enabled = false;
    g_blitz_setup.blessing_count = 0;
    g_blitz_setup.curse_count = 0;
    g_blitz_setup.effect_mode = BLITZ_EFFECT_RANDOM;
}

blitz_setup* blitz_current_setup_mutable(void)
{
    return &g_blitz_setup;
}

const blitz_setup* blitz_current_setup(void)
{
    return &g_blitz_setup;
}

bool blitz_oaths_enabled(void)
{
    return run_mode_is_blitz() && g_blitz_setup.oaths_enabled;
}

bool blitz_auto_allocates_stats(void)
{
    return run_mode_is_blitz()
        && g_blitz_setup.character_mode == BLITZ_CHARACTER_RANDOM;
}

void blitz_runtime_reset(void)
{
    memset(g_blitz_curse_stacks, 0, sizeof(g_blitz_curse_stacks));
    g_blitz_curses_seen = 0;
    g_blitz_end_summary_shown = false;
}

int8_t* blitz_runtime_curse_stacks(void)
{
    return g_blitz_curse_stacks;
}

u64b* blitz_runtime_curses_seen(void)
{
    return &g_blitz_curses_seen;
}

void blitz_runtime_restore(const int8_t* stacks, u64b seen)
{
    if (stacks)
        memcpy(g_blitz_curse_stacks, stacks, sizeof(g_blitz_curse_stacks));
    else
        memset(g_blitz_curse_stacks, 0, sizeof(g_blitz_curse_stacks));

    g_blitz_curses_seen = seen;
    g_blitz_end_summary_shown = false;
}

void blitz_show_end_summary(byte sil_count)
{
    if (g_blitz_end_summary_shown)
        return;

    g_blitz_end_summary_shown = true;
    (void)blitz_show_end_summary_ui(sil_count);
}
