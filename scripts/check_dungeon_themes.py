#!/usr/bin/env python3
"""Compile and exercise the production dungeon theme parser and startup loader."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/dungeon-themes-check"

HARNESS = r'''
#include "level-generation/level-generation-themes.c"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

cptr ANGBAND_DIR_EDIT;
bool path_build(char* out, size_t size, cptr directory, cptr file)
{
    int written = snprintf(out, size, "%s/%s", directory, file);
    return written >= 0 && (size_t)written < size;
}
void log_log(int level, const char* file, int line, const char* fmt, ...)
{
    (void)level; (void)file; (void)line;
    va_list args;
    va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args);
    fputc('\n', stderr);
}
int main(int argc, char** argv)
{
    assert(argc == 3);
    bool expected = atoi(argv[2]) != 0;
    ANGBAND_DIR_EDIT = argv[1];
    assert(terrain_theme_for_depth(0)->chance == 0);
    assert(terrain_theme_for_depth(-1)->chance == 0);
    assert(terrain_theme_for_depth(21)->chance == 0);
    assert(terrain_landmark_for_depth(0)->chance == 0);
    assert(terrain_landmark_for_depth(-1)->chance == 0);
    assert(terrain_landmark_for_depth(21)->chance == 0);
    for (int depth = -1; depth <= 21; depth++)
        if (depth < 1 || depth > 20)
            for (int family = 0; family < TERRAIN_NETWORK_FAMILY_MAX; family++)
                assert(terrain_network_for_depth(depth)->weights[family] == 0);
    assert(terrain_themes_load() == expected);
    /* The normal loader is cached; generation never needs to reread the file. */
    ANGBAND_DIR_EDIT = "no-such-directory";
    assert(terrain_themes_load() == expected);
    for (int depth = 1; depth <= 20; depth++)
    {
        const terrain_theme_profile* profile = terrain_theme_for_depth(depth);
        printf("D:%d:%s", depth, profile->name);
        for (int material = 0; material < TERRAIN_THEME_MATERIAL_MAX; material++)
            printf(":%d", profile->weights[material]);
        printf(":%d:%d:%d:%d:%d\n", profile->chance, profile->min_length,
            profile->max_length, profile->max_width, profile->pool_chance);
    }
    for (int depth = 1; depth <= 20; depth++)
    {
        const terrain_landmark_profile* landmark = terrain_landmark_for_depth(depth);
        printf("M:%d:%d:%d:%d:%d:%d:%d:%d\n", depth, landmark->chance,
            landmark->min_span_percent, landmark->max_span_percent,
            landmark->min_width, landmark->max_width,
            landmark->lake_chance, landmark->branch_chance);
    }
    /* A valid record followed by invalid input must never partially commit. */
    for (int depth = 1; depth <= 20; depth++)
    {
        const terrain_network_profile* network = terrain_network_for_depth(depth);
        printf("N:%d", depth);
        for (int family = 0; family < TERRAIN_NETWORK_FAMILY_MAX; family++)
            printf(":%d", network->weights[family]);
        printf(":%d:%d:%d:%d:%d\n", network->min_basins, network->max_basins,
            network->min_radius, network->max_radius, network->one_tile_percent);
    }
    terrain_theme_profile before[20];
    memcpy(before, terrain_theme_profiles, sizeof(before));
    terrain_landmark_profile before_landmarks[20];
    memcpy(before_landmarks, terrain_landmark_profiles, sizeof(before_landmarks));
    terrain_network_profile before_networks[20];
    memcpy(before_networks, terrain_network_profiles, sizeof(before_networks));
    terrain_history_profile before_histories[20];
    memcpy(before_histories, terrain_history_profiles, sizeof(before_histories));
    assert(terrain_history_for_depth(0)->ancient == 0);
    assert(terrain_history_for_depth(21)->disaster == 0);
    for (int depth = 1; depth <= 20; depth++) {
        const terrain_history_profile* history = terrain_history_for_depth(depth);
        printf("H:%d:%d:%d:%d:%d\n", depth, history->ancient, history->disaster,
            history->overflow, history->second_system_chance);
    }
    const char* bad = "V:3\nD:3:Changed:0:0:1:0:0:100:6:48:3:100\n"
        "M:3:100:35:95:1:12:100:100\nN:3:1:2:3:4:5:1:6:2:12:100\nBAD\n";
    assert(!terrain_themes_parse(bad, strlen(bad), "transaction-fixture"));
    assert(!memcmp(before, terrain_theme_profiles, sizeof(before)));
    assert(!memcmp(before_landmarks, terrain_landmark_profiles, sizeof(before_landmarks)));
    assert(!memcmp(before_networks, terrain_network_profiles, sizeof(before_networks)));
    assert(!memcmp(before_histories, terrain_history_profiles, sizeof(before_histories)));
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source, exe = OUT / "check.c", OUT / "check.exe"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        str(ROOT / "build-standard/_deps/SDL"),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-std=c17", "-O1", "-g", "-Wall", "-Wextra",
        "-ffunction-sections", "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), "_deps/SDL/libSDL3.dll.a", "-Wl,--gc-sections", "-o", str(exe)],
        cwd=ROOT / "build-standard", env=env, check=True)

    shipped = (ROOT / "lib/edit/dungeon-themes.txt").read_text(encoding="utf-8")
    defaults = [line for line in shipped.splitlines() if line.startswith("D:")]
    landmark_defaults = [line for line in shipped.splitlines() if line.startswith("M:")]
    network_defaults = [line for line in shipped.splitlines() if line.startswith("N:")]
    history_defaults = [line for line in shipped.splitlines() if line.startswith("H:")]
    assert len(defaults) == len(landmark_defaults) == len(network_defaults) == 20
    count = 0

    def check(name, content, valid=False, expected=None, landmarks=None, networks=None, histories=None):
        nonlocal count
        folder = OUT / name
        folder.mkdir(exist_ok=True)
        path = folder / "dungeon-themes.txt"
        if content is not None:
            path.write_bytes(content.encode("utf-8") if isinstance(content, str) else content)
        elif path.exists():
            path.unlink()
        result = subprocess.run([str(exe), str(folder), str(int(valid))],
                                env=env, text=True, capture_output=True, timeout=10)
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        wanted = ((expected or defaults) + (landmarks or landmark_defaults)
                  + (networks or network_defaults) + (histories or history_defaults))
        assert result.stdout.splitlines() == wanted, (name, result.stdout)
        if not valid:
            assert "using built-in defaults" in result.stderr, (name, result.stderr)
            assert "dungeon-themes.txt:" in result.stderr, (name, result.stderr)
        count += 1

    check("shipped", shipped, True)
    history_custom = history_defaults.copy()
    history_custom[2] = "H:3:0:1000:0:100"
    check("history-custom", shipped.replace(history_defaults[2], history_custom[2]), True,
          histories=history_custom)
    check("history-omitted", shipped.replace(history_defaults[2], ""), True)
    check("history-duplicate", shipped + "\n" + history_defaults[2])
    for name, record in [
        ("zero-weights", "H:3:0:0:0:35"), ("high-weight", "H:3:1001:30:25:35"),
        ("high-chance", "H:3:45:30:25:101"), ("negative", "H:3:-1:30:25:35"),
        ("signed", "H:3:+1:30:25:35"), ("extra-field", "H:3:45:30:25:35:1"),
        ("missing-field", "H:3:45:30:25"), ("bad-depth", "H:21:45:30:25:35"),
    ]:
        check("history-" + name, shipped.replace(history_defaults[2], record))
    check("missing", None)
    check("empty", "")
    check("crlf-bom", b"\xef\xbb\xbf" + shipped.replace("\n", "\r\n").encode(), True)
    check("no-final-newline", shipped.rstrip("\n"), True)
    check("legacy-v1", "V:1\n" + "\n".join(defaults), True)
    legacy_v2 = "V:2\n" + "\n".join(defaults + landmark_defaults)
    check("legacy-v2", legacy_v2, True)
    check("legacy-v2-reordered", "V:2\n" + "\n".join(reversed(defaults + landmark_defaults)), True)
    check("reordered-depths", "V:3\n" + "\n".join(reversed(defaults + landmark_defaults + network_defaults)), True)
    check("v1-rejects-landmarks", legacy_v2.replace("V:2", "V:1", 1))
    check("v1-rejects-networks", "V:1\n" + "\n".join(defaults + network_defaults))
    check("v2-rejects-networks", shipped.replace("V:3", "V:2", 1))
    check("v2-missing-landmark", legacy_v2.replace(landmark_defaults[8], ""))
    check("missing-header", shipped.replace("V:3", "", 1))
    check("unknown-version", shipped.replace("V:3", "V:4", 1))
    check("duplicate-header", shipped + "\nV:3\n")
    check("missing-depth", shipped.replace(defaults[8], ""))
    check("duplicate-depth", shipped + "\n" + defaults[2])
    check("unexpected-directive", shipped + "\nX:foo\n")
    check("embedded-nul", shipped + "\0")
    check("oversized-file", shipped + "#" * 65536)
    check("oversized-line", shipped + "\n#" + "a" * 512)

    def replacement(name, index, value):
        fields = defaults[2].split(":")
        fields[index] = value
        check(name, shipped.replace(defaults[2], ":".join(fields)))

    for name, index, value in [
        ("negative-depth", 1, "-1"), ("zero-depth", 1, "0"), ("high-depth", 1, "21"),
        ("empty-name", 2, ""), ("long-name", 2, "a" * 48), ("blank-name", 2, "  "),
        ("invalid-name", 2, "name!"), ("negative-weight", 3, "-1"),
        ("signed-weight", 3, "+1"), ("float-weight", 3, "1.2"),
        ("junk-weight", 3, "1suffix"), ("space-weight", 3, " 1"),
        ("overflow-weight", 3, "9" * 60), ("high-weight", 3, "1001"),
        ("empty-weight", 3, ""), ("high-chance", 8, "101"),
        ("short-length", 9, "5"), ("long-length", 10, "49"),
        ("inverted-length", 9, "12"), ("zero-width", 11, "0"),
        ("high-width", 11, "4"), ("high-pool-chance", 12, "101"),
    ]:
        replacement(name, index, value)
    check("extra-field", shipped.replace(defaults[2], defaults[2] + ":1"))
    check("missing-field", shipped.replace(defaults[2], defaults[2].rsplit(":", 1)[0]))
    check("zero-active-weights", shipped.replace(defaults[2],
          "D:3:No material:0:0:0:0:0:100:6:48:3:100"))
    custom = defaults.copy()
    custom[2] = "D:3:Custom ice:0:0:0:0:1000:100:6:48:3:100"
    check("custom-material-and-bounds", shipped.replace(defaults[2], custom[2]), True, custom)
    check("legacy-v1-custom", "V:1\n" + "\n".join(custom), True, custom)
    old_landmarks = landmark_defaults.copy()
    old_landmarks[2] = "M:3:100:35:95:3:12:100:100"
    check("legacy-v2-custom", "V:2\n" + "\n".join(custom + old_landmarks),
          True, custom, old_landmarks)
    custom[2] = "D:3:Disabled:0:0:0:0:0:0:48:48:1:0"
    check("enabled-landmark-zero-weights", shipped.replace(defaults[2], custom[2]))
    disabled_landmarks = landmark_defaults.copy()
    disabled_landmarks[2] = "M:3:0:35:95:2:12:0:0"
    check("disabled-custom", shipped.replace(defaults[2], custom[2]).replace(
        landmark_defaults[2], disabled_landmarks[2]), True, custom, disabled_landmarks)
    check("legacy-disabled-custom", "V:1\n" + "\n".join(custom), True, custom)
    check("missing-landmark", shipped.replace(landmark_defaults[8], ""))
    check("duplicate-landmark", shipped + "\n" + landmark_defaults[2])
    check("extra-landmark-field", shipped.replace(landmark_defaults[2], landmark_defaults[2] + ":0"))
    check("missing-landmark-field", shipped.replace(landmark_defaults[2], landmark_defaults[2].rsplit(":", 1)[0]))
    for name, index, value in [
        ("negative-depth", 1, "-1"), ("zero-depth", 1, "0"), ("high-depth", 1, "21"),
        ("negative-chance", 2, "-1"), ("high-chance", 2, "101"),
        ("short-span", 3, "34"), ("long-span", 4, "96"),
        ("inverted-span", 3, "81"), ("zero-width", 5, "0"),
        ("wide-width", 6, "13"), ("inverted-width", 5, "8"),
        ("high-lake-chance", 7, "101"), ("high-branch-chance", 8, "101"),
        ("signed-span", 3, "+50"), ("empty-span", 3, ""),
        ("overflow-width", 5, "9" * 60), ("junk-width", 5, "3suffix"),
    ]:
        fields = landmark_defaults[2].split(":")
        fields[index] = value
        check("landmark-" + name, shipped.replace(landmark_defaults[2], ":".join(fields)))
    for name, record in [("minimum", "M:3:0:35:35:1:1:0:0"),
                         ("maximum", "M:3:100:95:95:12:12:100:100")]:
        custom_landmarks = landmark_defaults.copy()
        custom_landmarks[2] = record
        check("landmark-" + name, shipped.replace(landmark_defaults[2], record),
              True, landmarks=custom_landmarks)
    check("missing-network", shipped.replace(network_defaults[8], ""))
    check("duplicate-network", shipped + "\n" + network_defaults[2])
    check("extra-network-field", shipped.replace(network_defaults[2], network_defaults[2] + ":0"))
    check("missing-network-field", shipped.replace(network_defaults[2], network_defaults[2].rsplit(":", 1)[0]))
    for name, index, value in [
        ("negative-depth", 1, "-1"), ("zero-depth", 1, "0"), ("high-depth", 1, "21"),
        ("negative-weight", 2, "-1"), ("signed-weight", 3, "+1"),
        ("float-weight", 4, "1.2"), ("empty-weight", 5, ""),
        ("junk-weight", 6, "1suffix"), ("space-weight", 2, " 1"),
        ("overflow-weight", 3, "9" * 60), ("high-weight", 4, "1001"),
        ("zero-basins", 7, "0"), ("high-basins", 8, "7"),
        ("inverted-basins", 7, "5"), ("small-radius", 9, "1"),
        ("large-radius", 10, "13"), ("inverted-radius", 9, "7"),
        ("negative-narrow", 11, "-1"), ("high-narrow", 11, "101"),
    ]:
        fields = network_defaults[2].split(":")
        fields[index] = value
        check("network-" + name, shipped.replace(network_defaults[2], ":".join(fields)))
    for name, record in [
        ("minimum", "N:3:1:0:0:0:0:1:1:2:2:0"),
        ("maximum", "N:3:1000:1000:1000:1000:1000:6:6:12:12:100"),
        ("custom", "N:3:0:0:0:0:1000:1:6:2:12:55"),
    ]:
        custom_networks = network_defaults.copy()
        custom_networks[2] = record
        check("network-" + name, shipped.replace(network_defaults[2], record),
              True, networks=custom_networks)
    custom_networks = network_defaults.copy()
    custom_networks[2] = "N:3:0:0:0:0:0:1:6:2:12:0"
    no_families = shipped.replace(network_defaults[2], custom_networks[2])
    check("network-enabled-zero-weights", no_families)
    check("network-disabled-zero-weights", no_families.replace(
        landmark_defaults[2], disabled_landmarks[2]), True,
        landmarks=disabled_landmarks, networks=custom_networks)
    print(f"Dungeon themes: {count} production-parser cases; shipped/default parity, "
          "strict bounds, custom profiles, startup fallback and transactional rejection: PASS")


if __name__ == "__main__":
    main()
