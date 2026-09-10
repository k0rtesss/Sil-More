#!/usr/bin/env python3
"""Exercise the production grouped gameplay-tutorial archive in isolation.

The archive menu is extracted from tutorial-game.c so this check keeps the
real topic mapping, sorting, filtering, pagination, and replay loop.  The
question overlay and terminal event pump are scripted test doubles; no game
window, save, or player preference is opened.
"""
from pathlib import Path
import json
import os
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-archive-check"


def c_literal(value):
    """Return a JSON string as a valid C string literal."""
    text = json.dumps(value, separators=(",", ":"))
    return json.dumps(text)


def make_catalogue():
    cards = []

    def add(lesson_id, title, priority=1, level=None):
        lesson = {
            "id": lesson_id,
            "title": title,
            "priority": priority,
            "steps": [{"text": title}],
        }
        if level:
            lesson["level"] = level
        cards.append(lesson)

    add("opening.start", "Getting started")
    add("menu.main", "Menus")
    for number in range(1, 12):
        add(f"combat.{number:02d}", f"Combat {number:02d}", priority=20 - number)
    add("item.weapon", "Weapon basics")
    add("identification.item", "Identifying items")
    add("storage.pack", "Carrying things")
    add("ability.1", "Using abilities")
    add("advancement.skills", "Advancing skills")
    add("world.map", "Exploring")
    add("terrain.lava", "Terrain hazards")
    add("monster.orc", "Monsters")
    for number in range(1, 11):
        add(f"status.{number:02d}", f"Condition {number:02d}")
    add("effect.fire", "Item effects")
    add("quest.main", "Questing")
    add("tale.goal", "Tale goals")
    add("unknown.card", "Other card")
    add("unknown.queue", "Queued but unseen", priority=1)
    add("unknown.extended", "Extended card", level="extended")
    add("unknown.unseen", "Unseen card")
    return {"version": 1, "lessons": cards}


def make_progress(statuses):
    return {
        "version": 1,
        "lessons": {
            lesson_id: {"status": status, "step": 0 if status != "completed" else 1}
            for lesson_id, status in statuses.items()
        },
    }


CATALOGUE = make_catalogue()
RICH_STATUSES = {}
for lesson in CATALOGUE["lessons"]:
    lesson_id = lesson["id"]
    if lesson_id in {"unknown.queue", "unknown.extended", "unknown.unseen"}:
        continue
    RICH_STATUSES[lesson_id] = "completed"
RICH_STATUSES["combat.01"] = "skipped"
RICH_STATUSES["combat.02"] = "in-progress"
RICH_STATUSES["status.01"] = "skipped"
RICH_STATUSES["status.02"] = "in-progress"
RICH_PROGRESS = make_progress(RICH_STATUSES)
EXTENDED_PROGRESS = make_progress({"unknown.extended": "completed"})


HARNESS = r'''
#include "angband.h"
#include "tutorial/tutorial.c"
#include "ui/question.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

static metarun test_tale;
static bool test_blitz;
cptr ANGBAND_DIR_HELP = ".";
const metarun *metarun_current(void) { return &test_tale; }
bool run_mode_is_blitz(void) { return test_blitz; }
void ensure_run_dir(const metarun *m) { (void)m; }
bool build_meta_path(char *buf, size_t size, const metarun *m, const char *leaf)
{ return SDL_snprintf(buf, size, "tale-%u-%s", m->id, leaf) < (int)size; }
bool path_build(char *buf, size_t size, const char *base, const char *leaf)
{ return SDL_snprintf(buf, size, "%s/%s", base, leaf) < (int)size; }
size_t strnfmt(char *buf, size_t max, const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    int size = SDL_vsnprintf(buf, max, fmt, args);
    va_end(args); return size < 0 ? 0 : (size_t)size;
}
void log_log(int level, const char *file, int line, const char *fmt, ...)
{ (void)level; (void)file; (void)line; (void)fmt; }
void msg_print(cptr message) { (void)message; }

static bool waiting;
static bool managing;
static int wait_calls;
static int fresh_calls;
static int event_calls;
static bool replay_seen;

static void tutorial_game_wait(void)
{
    assert(!managing && !waiting);
    ++wait_calls;
}

typedef struct ui_call {
    char title[160];
    char description[256];
    int count;
    int default_index;
    char labels[16][220];
    bool disabled[16];
} ui_call;

static ui_call calls[64];
static int call_count;
static const int *script;
static int script_count;
static int script_position;

static void use_script(const int *choices, int count)
{
    script = choices;
    script_count = count;
    script_position = 0;
    call_count = 0;
    memset(calls, 0, sizeof(calls));
}

int ui_question_ask_overlay(cptr title, cptr description,
    const ui_question_option *options, int count, int anchor_y, int anchor_x,
    int default_index)
{
    ui_call *call;
    assert(managing);
    assert(anchor_y == UI_QUESTION_GLOBAL && anchor_x == UI_QUESTION_GLOBAL);
    assert(call_count < (int)N_ELEMENTS(calls));
    call = &calls[call_count++];
    SDL_strlcpy(call->title, title, sizeof(call->title));
    SDL_strlcpy(call->description, description, sizeof(call->description));
    call->count = count;
    call->default_index = default_index;
    assert(count <= (int)N_ELEMENTS(call->labels));
    for (int i = 0; i < count; ++i) {
        SDL_strlcpy(call->labels[i], options[i].label, sizeof(call->labels[i]));
        call->disabled[i] = options[i].disabled;
    }
    assert(script_position < script_count);
    int choice = script[script_position++];
    assert(choice >= -1 && choice < count);
    return choice;
}

errr Term_fresh(void)
{
    assert(waiting);
    ++fresh_calls;
    return 0;
}

errr Term_xtra(int event, int value)
{
    tutorial_view view;
    assert(waiting && event == TERM_XTRA_EVENT && value == 1);
    ++event_calls;
    assert(tutorial_get_view(&view));
    if (!strcmp(view.id, "combat.01")) replay_seen = true;
    tutorial_continue();
    return 0;
}

static void write_text(const char *path, const char *text)
{
    SDL_IOStream *file = SDL_IOFromFile(path, "wb");
    assert(file);
    assert(SDL_WriteIO(file, text, strlen(text)) == strlen(text));
    assert(SDL_CloseIO(file));
}

static void select_tale(u32b id, const char *progress)
{
    char path[96];
    test_tale.id = id;
    SDL_snprintf(path, sizeof(path), "tale-%u-tutorials.json", id);
    SDL_RemovePath(path);
    if (progress) write_text(path, progress);
    tutorial_sync_tale();
}

static void assert_rich_topics(void)
{
    const ui_call *call = &calls[0];
    const char *expected[] = {
        "Getting started & menus (2)", "Combat (11)",
        "Items & equipment (2)", "Carrying & storage (1)",
        "Skills & abilities (2)", "Exploration (1)",
        "Terrain & traps (1)", "Monsters (1)", "Conditions (10)",
        "Item effects (1)", "Quests & Tales (2)", "Other (1)",
    };
    assert(!strcmp(call->title, "Tutorial cards"));
    assert(call->count == 13 && strstr(call->description, "35 revealed"));
    for (int i = 0; i < 12; ++i) assert(!strcmp(call->labels[i], expected[i]));
    assert(!strcmp(call->labels[12], "Back"));
}

static void assert_empty_topics(void)
{
    const ui_call *call = &calls[0];
    assert(!strcmp(call->title, "Tutorial cards"));
    assert(call->count == 1 && !strcmp(call->labels[0], "Back"));
    assert(strstr(call->description, "No tutorial cards"));
}

static void assert_extended_topics(void)
{
    const ui_call *call = &calls[0];
    assert(!strcmp(call->title, "Tutorial cards"));
    assert(call->count == 2 && !strcmp(call->labels[0], "Other (1)"));
    assert(!strcmp(call->labels[1], "Back"));
    assert(strstr(call->description, "1 revealed"));
}

__ARCHIVE_SECTION__

int main(void)
{
    static const int rich_choices[] = {8, 10, 12, 1, 10, 2, 12, 12};
    static const int replay_choices[] = {1, 0, 12, -1};
    static const int empty_choices[] = {-1};
    static const int extended_choices[] = {0, 3, 1};
    tutorial_view view;

    assert(SDL_Init(0));
    write_text("catalogue.json", __CATALOGUE__);
    assert(tutorial_load_catalogue("catalogue.json"));
    assert(tutorial_archive_count() == 38);

    /* The topic menu counts only revealed cards, and the card menu carries
     * skipped/in-progress state through sorting and page boundaries. */
    select_tale(701, __RICH_PROGRESS__);
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
    use_script(rich_choices, (int)N_ELEMENTS(rich_choices));
    tutorial_game_archive();
    assert(!managing && !waiting && wait_calls == 1);
    assert(call_count == 8 && script_position == script_count);
    assert_rich_topics();
    assert(calls[1].count == 13 && calls[1].disabled[10]);
    assert(calls[2].count == 13 && calls[2].disabled[10]);
    assert(!strcmp(calls[1].labels[0], "Condition 01 (skipped)"));
    assert(!strcmp(calls[1].labels[1], "Condition 02 (in progress)"));
    assert(!strcmp(calls[1].description, "Page 1 of 1. Select a card to read it again. Escape returns to topics."));
    assert(!strcmp(calls[4].title, "Tutorial cards: Combat"));
    assert(calls[4].count == 13 && !calls[4].disabled[10]);
    assert(calls[5].count == 4 && !calls[5].disabled[2]);
    assert(!strcmp(calls[5].description, "Page 2 of 2. Select a card to read it again. Escape returns to topics."));
    assert(calls[6].count == 13 && !calls[6].disabled[10]);

    /* Replaying a card runs the production Term event loop and leaves the
     * archive's active lesson and pending queue untouched. */
    tutorial_invalidate_context();
    tutorial_observe("combat.02", NULL);
    tutorial_observe("unknown.queue", NULL);
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "combat.02"));
    assert(queue_count == 1);
    tutorial_status old_status = tutorial_lesson_status("combat.01");
    replay_seen = false;
    fresh_calls = event_calls = 0;
    use_script(replay_choices, (int)N_ELEMENTS(replay_choices));
    tutorial_game_archive();
    assert(!managing && !waiting && wait_calls == 2);
    assert(replay_seen && fresh_calls > 0 && event_calls == fresh_calls);
    assert(tutorial_lesson_status("combat.01") == old_status);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "combat.02"));
    assert(queue_count == 1);

    /* An empty Tale produces no phantom topic. */
    select_tale(702, NULL);
    use_script(empty_choices, (int)N_ELEMENTS(empty_choices));
    tutorial_game_archive();
    assert(!managing && !waiting && wait_calls == 3);
    assert(call_count == 1 && script_position == script_count);
    assert_empty_topics();

    /* Archive cards remain readable when their lesson level is filtered out
     * of live observation, including with tutorials Disabled. */
    select_tale(703, __EXTENDED_PROGRESS__);
    tutorial_set_mode(TUTORIAL_MODE_NORMAL);
    use_script(extended_choices, (int)N_ELEMENTS(extended_choices));
    tutorial_game_archive();
    assert(!managing && !waiting && wait_calls == 4);
    assert_extended_topics();
    assert(!strcmp(calls[1].title, "Tutorial cards: Other"));
    assert(!strcmp(calls[1].labels[0], "Extended card"));
    assert(calls[1].count == 4 && calls[1].disabled[1] && calls[1].disabled[2]);

    select_tale(704, __EXTENDED_PROGRESS__);
    tutorial_set_mode(TUTORIAL_MODE_DISABLED);
    use_script(extended_choices, (int)N_ELEMENTS(extended_choices));
    tutorial_game_archive();
    assert(!managing && !waiting && wait_calls == 5);
    assert_extended_topics();
    assert(!strcmp(calls[1].labels[0], "Extended card"));

    tutorial_shutdown();
    SDL_Quit();
    puts("Tutorial archive: PASS (revealed filtering, 12 topics, status labels, exact-10/11 pagination, previous/back, empty, replay preservation, Normal/Disabled Extended cards)");
    return 0;
}
'''


def main():
    source_text = (ROOT / "src/tutorial/tutorial-game.c").read_text(encoding="utf-8")
    marker = "#define TUTORIAL_ARCHIVE_PAGE_SIZE 10"
    end_marker = "void tutorial_game_lifecycle(const char *id)"
    start = source_text.find(marker)
    end = source_text.find(end_marker, start)
    assert start >= 0, f"Missing archive implementation marker: {marker}"
    assert end > start, f"Missing archive implementation end marker: {end_marker}"
    archive = source_text[start:end]

    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    rendered = HARNESS.replace("__ARCHIVE_SECTION__", archive)
    rendered = rendered.replace("__CATALOGUE__", c_literal(CATALOGUE))
    rendered = rendered.replace("__RICH_PROGRESS__", c_literal(RICH_PROGRESS))
    rendered = rendered.replace("__EXTENDED_PROGRESS__", c_literal(EXTENDED_PROGRESS))
    source.write_text(rendered, encoding="utf-8")

    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        str(BUILD / "_deps/SDL"), env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-Wall", "-Wextra", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        str(ROOT / "src/cJSON.c"), "_deps/SDL/libSDL3.dll.a",
        "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
