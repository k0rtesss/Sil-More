/* runtime/runtime-cli.h - runtime command-line state and usage */

#ifndef INCLUDED_RUNTIME_CLI_H
#define INCLUDED_RUNTIME_CLI_H

#include "h-basic.h"

void runtime_cli_reset(void);
void runtime_cli_print_usage(cptr program_name);

bool runtime_cli_fiddle(void);
void runtime_cli_set_fiddle(bool enabled);

bool runtime_cli_wizard(void);
void runtime_cli_set_wizard(bool enabled);

bool runtime_cli_sound(void);
void runtime_cli_set_sound(bool enabled);

int runtime_cli_graphics_mode(void);
void runtime_cli_set_graphics_mode(int mode);

bool runtime_cli_force_original(void);
void runtime_cli_set_force_original(bool enabled);

bool runtime_cli_force_roguelike(void);
void runtime_cli_set_force_roguelike(bool enabled);

#endif /* INCLUDED_RUNTIME_CLI_H */
