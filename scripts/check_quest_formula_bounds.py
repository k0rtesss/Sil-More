#!/usr/bin/env python3
"""Check the production quest parser and probability calculator together."""

from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/quest-formula-bounds"


def probability_function():
    source = (ROOT / "src/level-generation/level-generation-quests.c").read_text(
        encoding="utf-8-sig")
    start = source.index("float calculate_parametric_probability(quest_type* q_ptr, int depth) {")
    end = source.index("\n}", start) + 2
    return source[start:end]


HARNESS = r'''
#include "angband.h"
#include "init/init-parse-internal.h"
#include "log/log.h"
#include <assert.h>
#include <math.h>

int error_idx = -1;
static maxima limits = { .oath_max = 16 };
maxima* z_info = &limits;
static quest_type quests[16];
static header head = { .info_num = 16, .info_ptr = quests };
errr parse_quest_info(char* buf, header* head);

/* Text storage and logging are irrelevant to eligibility/probability. */
u32b add_name(header* h, cptr text) { (void)h; (void)text; return 1; }
bool add_text(u32b* offset, header* h, cptr text)
{ (void)h; (void)text; *offset = 1; return true; }
void log_log(int level, const char* file, int line, const char* fmt, ...)
{ (void)level; (void)file; (void)line; (void)fmt; }

static void parse(const char* text)
{
    char buf[4096];
    assert(strlen(text) < sizeof(buf));
    snprintf(buf, sizeof(buf), "%s", text);
    assert(parse_quest_info(buf, &head) == 0);
}

static void close_to(float actual, float expected)
{
    if (fabsf(actual - expected) > 0.00001f)
    {
        fprintf(stderr, "Probability: expected %.6f, got %.6f\n", expected, actual);
        abort();
    }
}
'''

TESTS = r'''
int main(int argc, char** argv)
{
    assert(argc == 2);
    FILE* input = fopen(argv[1], "rb");
    assert(input);
    char line[4096];
    while (fgets(line, sizeof(line), input))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#' || line[0] == 'V') continue;
        parse(line);
    }
    fclose(input);

    /* Shipped records put E: before P:. Check both interpolation endpoints. */
    assert(quests[5].depth_min == 2 && quests[5].depth_max == 10);
    assert(quests[6].depth_min == 1 && quests[6].depth_max == 3);
    close_to(calculate_parametric_probability(&quests[5], 2), 0.05f);
    close_to(calculate_parametric_probability(&quests[5], 6), 0.0875f);
    close_to(calculate_parametric_probability(&quests[5], 10), 0.125f);
    close_to(calculate_parametric_probability(&quests[6], 1), 0.50f);
    close_to(calculate_parametric_probability(&quests[6], 2), 0.325f);
    close_to(calculate_parametric_probability(&quests[6], 3), 0.15f);
    close_to(calculate_parametric_probability(&quests[5], 1), 0.0f);
    close_to(calculate_parametric_probability(&quests[5], 11), 0.0f);
    close_to(calculate_parametric_probability(&quests[6], 0), 0.0f);
    close_to(calculate_parametric_probability(&quests[6], 4), 0.0f);

    /* Reversing directive order must have the same interpretation. */
    parse("Q:7:Order fixture");
    parse("P:LINEAR_INTERPOLATE:0.50:0.15:0:0");
    parse("E:DEPTH_RANGE:1:3");
    for (int depth = 0; depth <= 4; depth++)
        close_to(calculate_parametric_probability(&quests[7], depth),
            calculate_parametric_probability(&quests[6], depth));

    /* A second P: must not erase explicit bounds; no E: keeps defaults. */
    parse("P:LINEAR_INTERPOLATE:0.50:0.15:0:0");
    assert(quests[7].depth_min == 1 && quests[7].depth_max == 3);
    parse("Q:8:Default fixture");
    parse("P:FIXED_PERCENT:0.25:0:0:0");
    assert(quests[8].depth_min == 0 && quests[8].depth_max == 25);
    close_to(calculate_parametric_probability(&quests[8], 25), 0.25f);
    close_to(calculate_parametric_probability(&quests[8], 26), 0.0f);
    puts("Quest formula bounds: shipped endpoints, midpoints, exclusion, line order and defaults: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "check.c"
    fixture.write_text(HARNESS + probability_function() + TESTS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(ROOT / "build-standard/_deps/SDL"),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env.get("PATH", "")])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(fixture),
        str(ROOT / "src/init/init-parse-quests.c"),
        str(ROOT / "build-standard/_deps/SDL/libSDL3.dll.a"), "-o", str(exe)],
        cwd=ROOT / "build-standard", env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/edit/quest.txt")],
        env=env, check=True, timeout=15)


if __name__ == "__main__":
    main()
