#!/usr/bin/env python3
"""Exercise compact quest replay and monster recall against real SDL/game data."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/minimum-quest-monster"

CHECKS = r'''
/* QUEST_STATUS_SOURCE */
static int question_step, quest_choice, phase_choice, story_calls;
static bool expect_completion_enabled;
static char story_text[16384], blocks[16384];
static int recall_stage;
static int journal_input;

int __wrap_ui_question_ask_overlay(cptr title, cptr desc,
    const ui_question_option* options, int count, int ay, int ax, int initial)
{
    assert(ay == UI_QUESTION_GLOBAL && ax == UI_QUESTION_GLOBAL && initial == 0);
    assert(title && desc && options);
    if (question_step++ == 0) {
        assert(!strcmp(title, "Quest stories") && count >= 1);
        assert(quest_choice >= 0 && quest_choice < count);
        return quest_choice;
    }
    assert(count == 2 && !strcmp(options[0].label, "Introduction"));
    assert(options[1].disabled == !expect_completion_enabled);
    return phase_choice;
}
void __wrap_quest_typewriter_menu(cptr title, cptr texts[], int count,
    byte title_color, byte text_color)
{
    (void)title_color; (void)text_color;
    assert(title && count > 0);
    ++story_calls; story_text[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (texts[i]) SDL_strlcat(story_text, texts[i], sizeof(story_text));
        SDL_strlcat(story_text, "\n", sizeof(story_text));
    }
}
void __wrap_sdl_hint_quest_menu_add_block(cptr text, byte attr, int indent, int choice)
{
    (void)attr; (void)indent; (void)choice;
    SDL_strlcat(blocks, text ? text : "", sizeof(blocks));
    SDL_strlcat(blocks, "\n", sizeof(blocks));
}
void __wrap_message_flush(void) {}
char __wrap_inkey(void)
{
    if (journal_input) {
        int mode = journal_input; journal_input = 0;
        if (mode == 2) {
            bool wake = false;
            assert(ui_menu_click_handle_choice_action(QUEST_STATUS_CLICK_STORY,
                UI_MENU_CLICK_PRIMARY, &wake));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        return 's';
    }
    assert(recall_stage > 0 && recall_stage <= 3);
    return recall_stage < 3 ? 'x' : ESCAPE;
}
bool __real_sdl_description_overlay_present(const byte* attrs, const char* chars,
    const byte* tattrs, const char* tchars, const byte* story, const byte* health,
    int width, int height, int cols, int scroll, bool interactive,
    int* visible, int* max_scroll);
bool __wrap_sdl_description_overlay_present(const byte* attrs, const char* chars,
    const byte* tattrs, const char* tchars, const byte* story, const byte* health,
    int width, int height, int cols, int scroll, bool interactive,
    int* visible, int* max_scroll)
{
    char content[65536]; size_t pos = 0;
    (void)attrs; (void)tattrs; (void)tchars; (void)story; (void)health;
    (void)cols; (void)scroll;
    assert(interactive && width > 0 && height > 0);
    for (int y = 0; y < height && pos + width + 2 < sizeof(content); y++) {
        memcpy(content + pos, chars + y * width, width); pos += width;
        content[pos++] = '\n';
    }
    content[pos] = '\0';
    ++recall_stage;
    if (g_description_overlay.footer_action_count != 2)
        fprintf(stderr, "recall stage=%d mode=%d footer=%d '%s'\n", recall_stage,
            minimum_text_mode, g_description_overlay.footer_action_count,
            g_description_overlay.footer_text);
    assert(g_description_overlay.footer_action_count == 2);
    assert(g_description_overlay.footer_actions[0].key == 'x');
    if (recall_stage == 1 || recall_stage == 3) {
        assert(strstr(content, "Resists:"));
        assert(strstr(g_description_overlay.footer_text, "Full details"));
    } else {
        assert(!strstr(content, "Resists:"));
        assert(strstr(g_description_overlay.footer_text, "Compact"));
    }
    assert(__real_sdl_description_overlay_present(attrs, chars, tattrs, tchars,
        story, health, width, height, cols, scroll, interactive, visible, max_scroll));
    assert(SDL_SetRenderTarget(g_state.renderer, NULL));
    assert(sdl_render_current_window_frame());
    SDL_Surface* pixels = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(pixels);
    char path[1024];
    strnfmt(path, sizeof(path), "%s/monster-%d.png",
        SDL_getenv("MINIMUM_TEST_OUTPUT"), recall_stage);
    assert(IMG_SavePNG(pixels, path));
    SDL_DestroySurface(pixels);
    return true;
}

static void tests(void)
{
    s32b old_turn = turn, old_playerturn = playerturn;
    u64b rng_before;
    int before;
    op_ptr->opt[OPT_minimum_text_mode] = true;
    p_ptr->playing = true;
    p_ptr->tulkas_quest = TULKAS_QUEST_ACTIVE;
    p_ptr->tulkas_target_r_idx = 51;
    p_ptr->tulkas_prize_a_idx = 1;
    p_ptr->aule_quest = AULE_QUEST_ACTIVE;
    p_ptr->niena_quest = NIENA_QUEST_FAILED;
    p_ptr->orome_quest = OROME_QUEST_ACTIVE;
    p_ptr->orome_wolves_killed = 12;
    p_ptr->orome_spiders_killed = 7;
    p_ptr->orome_serpents_killed = 4;
    p_ptr->orome_vampires_killed = 1;
    p_ptr->varda_quest = VARDA_QUEST_SUCCESS;

    rng_before = Rand_state_export();
    question_step = quest_choice = phase_choice = 0;
    expect_completion_enabled = false;
    quest_status_show_story();
    assert(question_step == 2 && story_calls == 1);
    assert(strstr(story_text, r_name + r_info[51].name));
    assert(!strstr(story_text, "[monster name]") && !strstr(story_text, "[artifact name]"));
    assert(Rand_state_export() == rng_before);

    question_step = 0; quest_choice = 4; phase_choice = 1;
    expect_completion_enabled = true;
    quest_status_show_story();
    assert(story_calls == 2 && strstr(story_text, "stolen light"));

    before = story_calls;
    question_step = 0; quest_choice = 2; phase_choice = 1;
    expect_completion_enabled = false;
    quest_status_show_story();
    assert(story_calls == before);
    question_step = 0; phase_choice = -1;
    quest_status_show_story();
    assert(story_calls == before);

    p_ptr->tulkas_quest = TULKAS_QUEST_REWARDED;
    question_step = quest_choice = phase_choice = 0;
    expect_completion_enabled = true;
    quest_status_show_story();
    assert(story_calls == before + 1);
    assert(strstr(story_text, r_name + r_info[51].name));
    assert(!strstr(story_text, "the named foe"));

    /* Old saves may have cleared these historical fields; still show the
     * story without exposing raw template placeholders. */
    p_ptr->tulkas_target_r_idx = p_ptr->tulkas_prize_a_idx = 0;
    question_step = quest_choice = phase_choice = 0;
    quest_status_show_story();
    assert(story_calls == before + 2);
    assert(strstr(story_text, "the named foe") && strstr(story_text, "the promised artefact"));
    assert(!strstr(story_text, "[monster name]") && !strstr(story_text, "[artifact name]"));

    int row = 0; blocks[0] = '\0';
    p_ptr->varda_quest = VARDA_QUEST_ACTIVE;
    rng_before = Rand_state_export();
    assert(quest_status_put_minimum(0, 80, 24, &row));
    assert(strstr(blocks, "Objective:") && strstr(blocks, "Reward:"));
    assert(strstr(blocks, "Failed") && strstr(blocks, "Wolves 12/100"));
    assert(strstr(blocks, "500 ft") && strstr(blocks, "300 XP"));
    assert(Rand_state_export() == rng_before);

    before = story_calls;
    question_step = quest_choice = phase_choice = 0;
    expect_completion_enabled = true;
    journal_input = true;
    assert(do_cmd_quest_status_page() == HINT_QUEST_PAGE_QUESTS);
    assert(!journal_input && question_step == 2 && story_calls == before + 1);

    assert(minimum_text_mode);
    recall_stage = 0;
    l_list[51].flags3 |= RF3_RES_FIRE;
    assert(screen_roff(51, NULL) == ESCAPE);
    assert(recall_stage == 3 && !g_description_overlay.footer_action_count);
    assert(turn == old_turn && playerturn == old_playerturn);
    op_ptr->opt[OPT_minimum_text_mode] = false;
    puts("Quest journal story/reward progress and monster compact/full toggle PASS");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static cptr make_path")]
    init = HARNESS[HARNESS.index("static cptr make_path"):HARNESS.index("    int last = ")]
    production = (ROOT / "src/quest/quest-status.c").read_text(encoding="utf-8").replace('#include "externs.h"\n', '')
    checks = CHECKS.replace("/* QUEST_STATUS_SOURCE */", production)
    source = OUT / "check.c"
    source.write_text(prefix + checks + init + "    tests(); sdl_quit_hook(NULL); return 0;\n}\n", encoding="utf-8")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
        if not p.endswith(("/src/main.c.obj", "/src/quest/quest-status.c.obj"))), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIO_DRIVER="dummy", CONTROL_TEST_ASSETS=str(ROOT / "lib"),
               MINIMUM_TEST_OUTPUT=str(OUT))
    exe = OUT / "check.exe"
    wraps = ("ui_question_ask_overlay", "quest_typewriter_menu", "message_flush",
             "inkey", "sdl_description_overlay_present", "sdl_hint_quest_menu_add_block")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + x for x in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="profile-", dir=OUT) as profile:
        env["CONTROL_TEST_PROFILE"] = profile
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT, env=env,
                       check=True, timeout=40)


if __name__ == "__main__":
    main()
