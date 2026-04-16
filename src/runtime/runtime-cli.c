/* File: runtime-cli.c */

#include "angband.h"
#include "runtime/runtime-cli.h"

struct runtime_cli_state {
    bool fiddle;
    bool wizard;
    bool sound;
    int graphics_mode;
    bool force_original;
    bool force_roguelike;
};

static struct runtime_cli_state g_runtime_cli = {
    false, false, false, GRAPHICS_NONE, false, false
};

void runtime_cli_reset(void)
{
    g_runtime_cli.fiddle = false;
    g_runtime_cli.wizard = false;
    g_runtime_cli.sound = false;
    g_runtime_cli.graphics_mode = GRAPHICS_NONE;
    g_runtime_cli.force_original = false;
    g_runtime_cli.force_roguelike = false;
}

void runtime_cli_print_usage(cptr program_name)
{
    cptr name = (program_name && program_name[0]) ? program_name : "sil";

    printf("Usage: %s [options] [-- sdl-options]\n", name);
    puts("  -n       Start a new character");
    puts("  -f       Request fiddle (verbose) mode");
    puts("  -w       Request wizard mode");
    puts("  -v       Request sound mode");
    puts("  -g       Request graphics mode");
    puts("  -o       Request original keyset (default)");
    puts("  -r       Request rogue-like keyset");
    puts("  -s<num>  Show <num> high scores (default: 10)");
    puts("  -u<who>  Use your <who> savefile");
    puts("  -d<def>  Define a 'lib' dir sub-path");
    puts("  SDL options after '--':");
    puts("     --scale <n>      Set the main view scale");
    puts("     --ascii          Force ASCII mode");
    puts("     --tiles          Force tile mode");
    puts("     --windowed       Force windowed mode");
    puts("     --fullscreen     Force fullscreen mode");
    puts("     --font-size <n>  Set the auxiliary view font size");
    puts("     --margin <n>     Set the SDL UI margin");
}

bool runtime_cli_fiddle(void)
{
    return g_runtime_cli.fiddle;
}

void runtime_cli_set_fiddle(bool enabled)
{
    g_runtime_cli.fiddle = enabled;
}

bool runtime_cli_wizard(void)
{
    return g_runtime_cli.wizard;
}

void runtime_cli_set_wizard(bool enabled)
{
    g_runtime_cli.wizard = enabled;
}

bool runtime_cli_sound(void)
{
    return g_runtime_cli.sound;
}

void runtime_cli_set_sound(bool enabled)
{
    g_runtime_cli.sound = enabled;
}

int runtime_cli_graphics_mode(void)
{
    return g_runtime_cli.graphics_mode;
}

void runtime_cli_set_graphics_mode(int mode)
{
    g_runtime_cli.graphics_mode = mode;
}

bool runtime_cli_force_original(void)
{
    return g_runtime_cli.force_original;
}

void runtime_cli_set_force_original(bool enabled)
{
    g_runtime_cli.force_original = enabled;
}

bool runtime_cli_force_roguelike(void)
{
    return g_runtime_cli.force_roguelike;
}

void runtime_cli_set_force_roguelike(bool enabled)
{
    g_runtime_cli.force_roguelike = enabled;
}
