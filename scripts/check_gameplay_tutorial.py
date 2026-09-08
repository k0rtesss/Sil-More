#!/usr/bin/env python3
"""Compile and exercise the real tutorial core in an isolated SDL/CJSON harness.

Uses the configured CMake include paths and SDL library. All progress writes go
to scripts/output/tutorial-check; no player preferences or Tales are opened.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "tutorial/tutorial.c"

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

static void write_text(const char *path, const char *text)
{
    SDL_IOStream *file = SDL_IOFromFile(path, "wb");
    assert(file);
    assert(SDL_WriteIO(file, text, strlen(text)) == strlen(text));
    assert(SDL_CloseIO(file));
}

static void offer(const char *id)
{ tutorial_observe(id, NULL); tutorial_checkpoint(true); }

int main(int argc, char **argv)
{
    tutorial_view view;
    tutorial_context context = {0};
    bool exists;
    cJSON *disk;
    assert(SDL_Init(0));
    assert(tutorial_get_mode()==TUTORIAL_MODE_EXTENDED);
    write_text("catalogue.json",
        "{\"version\":1,\"lessons\":["
        "{\"id\":\"move\",\"title\":\"Movement\",\"priority\":1,\"steps\":["
        "{\"text\":\"Look at {subject}: {context}\"},"
        "{\"text\":\"Move.\",\"action\":\"move\"},{\"text\":\"Movement complete.\"}]},"
        "{\"id\":\"attack\",\"title\":\"Attack\",\"priority\":10,\"steps\":["
        "{\"text\":\"Attack or move.\",\"kind\":\"action\",\"action\":\"attack|move\"}]},"
        "{\"id\":\"examine\",\"title\":\"Examine\",\"steps\":["
        "{\"text\":\"Examine a weapon.\",\"action\":\"examine\",\"subject_type\":\"weapon\"}]},"
        "{\"id\":\"purchase\",\"title\":\"Decision\",\"steps\":["
        "{\"text\":\"Read before purchasing.\",\"kind\":\"decision\"}]}]}"
    );
    write_text("tale-101-tutorials.json", "{\"version\":1,\"lessons\":{\"future-lesson\":{\"status\":\"completed\",\"extra\":9},\"item.skeleton\":{\"status\":\"completed\"},\"item.chest\":{\"status\":\"skipped\"}}}");
    test_tale.id = 101;
    assert(tutorial_load_catalogue("catalogue.json"));
    tutorial_sync_tale();
    SDL_strlcpy(context.subject, "visible orc", sizeof(context.subject));
    SDL_strlcpy(context.text, "public detail", sizeof(context.text));
    tutorial_observe("move", &context);
    memset(&context, 0, sizeof(context));
    tutorial_observe("attack", NULL);
    tutorial_observe("attack", NULL);
    assert(queue_count == 2);
    tutorial_checkpoint(false);
    assert(!tutorial_get_view(&view));
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "attack"));
    assert(!view.can_continue && view.kind == TUTORIAL_STEP_ACTION);
    assert(tutorial_action_permitted("attack"));
    assert(tutorial_action_permitted("move"));
    assert(tutorial_action_permitted("open-menu"));
    assert(!tutorial_action_permitted("use-item"));
    tutorial_action_finished("attack", NULL, false);
    tutorial_continue();
    assert(tutorial_is_active());
    tutorial_action_finished("attack", NULL, true); /* A miss is a committed attack. */
    assert(tutorial_lesson_status("attack") == TUTORIAL_COMPLETED);
    assert(!tutorial_is_active());
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && strstr(view.body, "visible orc: public detail"));
    assert(view.can_continue && !tutorial_action_permitted("move"));
    tutorial_continue();
    assert(!tutorial_is_active());
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && view.step == 2);
    tutorial_shutdown(); /* Restart resumes step only after another observation. */
    assert(tutorial_load_catalogue("catalogue.json"));
    tutorial_sync_tale();
    assert(!tutorial_is_active());
    offer("move");
    assert(tutorial_get_view(&view) && view.step == 2);
    tutorial_action_finished("move", NULL, true);
    tutorial_checkpoint(true); tutorial_continue();
    offer("examine");
    tutorial_action_finished("examine", "potion", true);
    assert(tutorial_is_active());
    tutorial_checkpoint(false); /* An authorized Pack transaction is in flight. */
    assert(!tutorial_is_active() && tutorial_action_waiting());
    tutorial_action_finished("examine", "weapon", true);
    assert(tutorial_lesson_status("examine") == TUTORIAL_COMPLETED);
    disk = read_json("tale-101-tutorials.json", &exists);
    assert(disk && cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(disk, "lessons"), "future-lesson"));
    cJSON_Delete(disk);
    test_tale.id = 102;
    SDL_RemovePath("tale-102-tutorials.json");
    tutorial_sync_tale();
    assert(tutorial_lesson_status("attack") == TUTORIAL_UNSEEN);
    offer("attack"); tutorial_skip();
    offer("attack"); assert(!tutorial_is_active());
    tutorial_reset_tale(); offer("attack"); assert(tutorial_is_active());
    tutorial_disable(); assert(!tutorial_is_active());
    assert(tutorial_replay("attack")); tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && view.can_continue && !tutorial_enabled());
    tutorial_continue(); assert(!tutorial_enabled());
    tutorial_set_enabled(true);
    test_blitz = true; offer("attack"); assert(!tutorial_is_active());
    test_blitz = false;
    test_tale.id = 101;
    tutorial_sync_tale();
    assert(tutorial_lesson_status("attack") == TUTORIAL_COMPLETED);
    assert(tutorial_replay("attack")); tutorial_checkpoint(true); tutorial_skip();
    assert(tutorial_lesson_status("attack") == TUTORIAL_COMPLETED);
    assert(tutorial_replay("attack")); tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && view.can_continue);
    assert(!tutorial_action_waiting() && !tutorial_action_permitted("attack"));
    tutorial_continue();
    assert(tutorial_lesson_status("attack") == TUTORIAL_COMPLETED);
    tutorial_reset_tale();
    assert(tutorial_lesson_status("future-lesson") == TUTORIAL_COMPLETED);
    assert(tutorial_lesson_status("item.skeleton") == TUTORIAL_COMPLETED);
    assert(tutorial_lesson_status("item.chest") == TUTORIAL_SKIPPED);
    assert(tutorial_lesson_status("move") == TUTORIAL_UNSEEN);
    write_text("bad.json", "{\"version\":1,\"lessons\":[{\"id\":\"bad\"}]}");
    assert(!tutorial_load_catalogue("bad.json"));
    assert(tutorial_archive_count() == 4);
    test_tale.id = 105;
    SDL_RemovePath("tale-105-tutorials.json");
    tutorial_sync_tale(); offer("purchase");
    assert(tutorial_get_view(&view) && view.can_continue && view.kind == TUTORIAL_STEP_DECISION);
    assert(!tutorial_action_permitted("purchase"));
    tutorial_action_finished("purchase", NULL, true);
    assert(tutorial_is_active());
    tutorial_continue(); assert(tutorial_lesson_status("purchase") == TUTORIAL_COMPLETED);
    offer("move"); tutorial_continue(); tutorial_checkpoint(true);
    tutorial_observe("attack", NULL);
    tutorial_action_finished("move", NULL, true);
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "attack"));
    tutorial_action_finished("attack", NULL, true);
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "move") && view.step == 3);
    tutorial_continue();
    test_tale.id = 106;
    SDL_RemovePath("tale-106-tutorials.json");
    tutorial_sync_tale();
    tutorial_observe("move", NULL); tutorial_observe("attack", NULL);
    tutorial_forget_observation("move");
    assert(queue_count == 1);
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "attack"));
    tutorial_forget_observation("attack");
    assert(!tutorial_is_active() && tutorial_lesson_status("attack") == TUTORIAL_IN_PROGRESS);
    offer("attack"); assert(tutorial_is_active()); tutorial_skip();
    test_tale.id = 103;
    write_text("tale-103-tutorials.json", "{broken");
    tutorial_sync_tale();
    offer("attack"); tutorial_skip();
    size_t len;
    char *raw = SDL_LoadFile("tale-103-tutorials.json", &len);
    assert(raw && !strcmp(raw, "{broken")); SDL_free(raw);
    test_tale.id = 104;
    write_text("tale-104-tutorials.json", "{\"version\":2,\"lessons\":{}}");
    tutorial_sync_tale(); offer("attack"); tutorial_skip();
    disk = read_json("tale-104-tutorials.json", &exists);
    assert(cJSON_GetObjectItemCaseSensitive(disk, "version")->valueint == 2);
    cJSON_Delete(disk);
    test_tale.id = 109;
    SDL_RemovePath("tale-109-tutorials.json");
    tutorial_sync_tale();
    SDL_strlcpy(context.subject, "original observation", sizeof(context.subject));
    tutorial_observe("move", &context); tutorial_checkpoint(true);
    tutorial_continue(); tutorial_checkpoint(true);
    tutorial_observe("attack", NULL);
    assert(tutorial_get_view(&view) && view.step == 2 && queue_count == 1);
    tutorial_archive_begin();
    assert(!tutorial_is_active() && queue_count == 0 && !tutorial_action_waiting());
    assert(tutorial_replay("move")); tutorial_checkpoint(true);
    while (tutorial_get_view(&view)) {
        assert(view.can_continue && view.kind == TUTORIAL_STEP_INFO);
        tutorial_continue(); tutorial_checkpoint(true);
    }
    assert(!tutorial_is_active()); /* Original is still suspended in archive. */
    assert(tutorial_lesson_status("move") == TUTORIAL_IN_PROGRESS);
    assert(tutorial_replay("attack")); tutorial_checkpoint(true); tutorial_skip();
    tutorial_observe("examine", NULL);
    assert(queue_count == 0 && tutorial_lesson_status("attack") == TUTORIAL_UNSEEN);
    tutorial_archive_end();
    assert(tutorial_get_view(&view) && !strcmp(view.id, "move") && view.step == 2);
    assert(!strcmp(view.context.subject, "original observation") && queue_count == 1);
    tutorial_action_finished("move", NULL, true); tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "attack"));
    tutorial_action_finished("attack", NULL, true); tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id, "move") && view.step == 3);
    tutorial_continue();
    assert(tutorial_lesson_status("move") == TUTORIAL_COMPLETED);
    test_tale.id = 110;
    SDL_RemovePath("tale-110-tutorials.json");
    tutorial_sync_tale(); offer("attack");
    tutorial_archive_begin(); tutorial_set_enabled(false);
    assert(tutorial_replay("move")); tutorial_checkpoint(true);
    while (tutorial_get_view(&view)) { tutorial_continue(); tutorial_checkpoint(true); }
    tutorial_archive_end();
    assert(!tutorial_enabled() && !tutorial_is_active() && queue_count == 0);
    tutorial_set_enabled(true); tutorial_checkpoint(true);
    assert(!tutorial_is_active()); /* Off discarded the suspended action. */
    assert(tutorial_lesson_status("attack") == TUTORIAL_IN_PROGRESS);
    offer("attack"); tutorial_archive_begin();
    assert(tutorial_replay("move")); tutorial_checkpoint(true);
    test_tale.id = 111;
    SDL_RemovePath("tale-111-tutorials.json");
    tutorial_archive_end();
    assert(!tutorial_is_active() && queue_count == 0);
    assert(tutorial_lesson_status("attack") == TUTORIAL_UNSEEN);
    offer("attack"); tutorial_checkpoint(false);
    tutorial_archive_begin();
    assert(tutorial_replay("move")); tutorial_checkpoint(true); tutorial_skip();
    tutorial_archive_end();
    assert(!tutorial_get_view(&view) && tutorial_action_waiting());
    assert(tutorial_peek_view(&view) && !strcmp(view.id, "attack"));
    tutorial_checkpoint(true); assert(tutorial_is_active()); tutorial_skip();
    write_text("double.json", "{\"version\":1,\"lessons\":[{\"id\":\"double\",\"title\":\"Two actions\",\"steps\":[{\"text\":\"First move\",\"action\":\"move\"},{\"text\":\"Second move\",\"action\":\"move\"}]}]}");
    assert(tutorial_load_catalogue("double.json"));
    test_tale.id = 108;
    SDL_RemovePath("tale-108-tutorials.json");
    tutorial_sync_tale(); offer("double");
    tutorial_action_finished("move", NULL, true);
    tutorial_action_finished("move", NULL, true); /* Duplicate producer for one commit. */
    assert(tutorial_lesson_status("double") == TUTORIAL_IN_PROGRESS);
    tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && view.step == 2);
    tutorial_action_finished("move", NULL, true);
    assert(tutorial_lesson_status("double") == TUTORIAL_COMPLETED);
    write_text("modes.json", "{\"version\":1,\"lessons\":["
        "{\"id\":\"basic\",\"title\":\"Basic\",\"level\":\"normal\",\"steps\":[{\"text\":\"Basic info\"}]},"
        "{\"id\":\"queued-normal\",\"title\":\"Queued basic\",\"level\":\"normal\",\"steps\":[{\"text\":\"Basic info\"}]},"
        "{\"id\":\"detail\",\"title\":\"Detail\",\"level\":\"extended\",\"steps\":[{\"text\":\"Detail info\"},{\"text\":\"Examine\",\"action\":\"examine\"}]},"
        "{\"id\":\"later\",\"title\":\"Later detail\",\"level\":\"extended\",\"steps\":[{\"text\":\"Later\"}]}]}");
    assert(tutorial_load_catalogue("modes.json"));
    test_tale.id=120; SDL_RemovePath("tale-120-tutorials.json"); tutorial_sync_tale();
    tutorial_set_mode(TUTORIAL_MODE_NORMAL);
    tutorial_set_enabled(true); assert(tutorial_get_mode()==TUTORIAL_MODE_NORMAL);
    offer("detail"); assert(!tutorial_is_active());
    assert(tutorial_lesson_status("detail")==TUTORIAL_UNSEEN);
    offer("basic"); assert(tutorial_get_view(&view) && view.level==TUTORIAL_MODE_NORMAL);
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
    assert(tutorial_get_view(&view) && !strcmp(view.id,"basic"));
    tutorial_continue();
    offer("detail"); tutorial_continue(); tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && view.level==TUTORIAL_MODE_EXTENDED && view.step==2);
    tutorial_observe("queued-normal",NULL); tutorial_observe("later",NULL);
    tutorial_set_mode(TUTORIAL_MODE_NORMAL);
    assert(!tutorial_is_active() && queue_count==1);
    assert(tutorial_lesson_status("detail")==TUTORIAL_IN_PROGRESS);
    assert(tutorial_lesson_status("later")==TUTORIAL_UNSEEN);
    tutorial_checkpoint(true); assert(tutorial_get_view(&view) && !strcmp(view.id,"queued-normal"));
    tutorial_continue();
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED); offer("detail");
    assert(tutorial_get_view(&view) && view.step==2);
    tutorial_action_finished("examine",NULL,true);
    assert(tutorial_lesson_status("detail")==TUTORIAL_COMPLETED);
    tutorial_set_mode(TUTORIAL_MODE_NORMAL); tutorial_archive_begin();
    assert(tutorial_replay("detail")); tutorial_checkpoint(true);
    while(tutorial_get_view(&view)) { assert(view.can_continue); tutorial_continue(); tutorial_checkpoint(true); }
    tutorial_archive_end(); assert(tutorial_lesson_status("detail")==TUTORIAL_COMPLETED);
    test_tale.id=121; SDL_RemovePath("tale-121-tutorials.json"); tutorial_sync_tale();
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED); offer("detail");
    tutorial_set_character_blocked(true);
    assert(tutorial_character_blocked() && tutorial_get_mode()==TUTORIAL_MODE_EXTENDED);
    offer("basic"); assert(!tutorial_is_active() && queue_count==0);
    assert(tutorial_lesson_status("basic")==TUTORIAL_UNSEEN);
    tutorial_set_mode(TUTORIAL_MODE_NORMAL); tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
    tutorial_reset_tale(); offer("detail");
    assert(tutorial_character_blocked() && !tutorial_is_active());
    tutorial_archive_begin(); assert(tutorial_replay("detail")); tutorial_checkpoint(true);
    while(tutorial_get_view(&view)) { assert(view.can_continue); tutorial_continue(); tutorial_checkpoint(true); }
    tutorial_archive_end(); assert(tutorial_character_blocked() && !tutorial_is_active());
    assert(tutorial_lesson_status("detail")==TUTORIAL_UNSEEN);
    tutorial_set_mode(TUTORIAL_MODE_DISABLED);
    assert(tutorial_cycle_mode()==TUTORIAL_MODE_NORMAL);
    assert(tutorial_cycle_mode()==TUTORIAL_MODE_EXTENDED);
    assert(tutorial_cycle_mode()==TUTORIAL_MODE_DISABLED);
    assert(!strcmp(tutorial_mode_name(TUTORIAL_MODE_DISABLED),"Disabled"));
    assert(!strcmp(tutorial_mode_name(TUTORIAL_MODE_NORMAL),"Normal"));
    assert(!strcmp(tutorial_mode_name(TUTORIAL_MODE_EXTENDED),"Extended"));
    tutorial_set_character_blocked(false); assert(!tutorial_enabled());
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED); offer("detail");
    tutorial_observe("queued-normal",NULL);
    tutorial_archive_begin(); assert(tutorial_replay("detail")); tutorial_checkpoint(true);
    tutorial_set_mode(TUTORIAL_MODE_NORMAL);
    assert(tutorial_get_view(&view) && view.can_continue); /* Archive ignores level filtering. */
    tutorial_skip(); tutorial_archive_end(); tutorial_checkpoint(true);
    assert(tutorial_get_view(&view) && !strcmp(view.id,"queued-normal"));
    assert(tutorial_lesson_status("detail")==TUTORIAL_IN_PROGRESS);
    tutorial_continue(); tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
    write_text("bad-level.json", "{\"version\":1,\"lessons\":[{\"id\":\"bad\",\"title\":\"Bad level\",\"level\":2,\"steps\":[{\"text\":\"Bad\"}]}]}");
    assert(!tutorial_load_catalogue("bad-level.json"));
    assert(tutorial_archive_count()==4);
    tutorial_shutdown();
    if (argc > 1) {
        int abilities = 0;
        assert(tutorial_load_catalogue(argv[1]));
        assert(tutorial_archive_count() > 100);
        for (int i = 0; i < tutorial_archive_count(); ++i) {
            assert(tutorial_archive_entry(i, &view, NULL));
            if (!strncmp(view.id, "ability.", 8)) ++abilities;
        }
        assert(abilities == 107);
        tutorial_shutdown();
        char help_directory[1024];
        SDL_strlcpy(help_directory, argv[1], sizeof(help_directory));
        char *separator = strrchr(help_directory, '\\');
        char *forward = strrchr(help_directory, '/');
        if (!separator || (forward && forward > separator)) separator = forward;
        assert(separator); *separator = '\0';
        ANGBAND_DIR_HELP = help_directory;
        assert(tutorial_archive_count() > 100); /* Real lazy installed-resource route. */
        tutorial_shutdown();
    }
    SDL_Quit();
    puts("Gameplay tutorial core: PASS (queue, safe checkpoints, actions, Tale persistence, restart, replay/reset, future IDs, corrupt/future data)");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        str(BUILD / "_deps/SDL"), env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-Wall", "-Wextra", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/cJSON.c"), "_deps/SDL/libSDL3.dll.a",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    catalogue = ROOT / 'lib/help/tutorials.json'
    command = [str(exe)] + ([str(catalogue)] if catalogue.exists() else [])
    subprocess.run(command, cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
