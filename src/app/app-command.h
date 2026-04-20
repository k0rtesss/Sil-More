/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#ifndef INCLUDED_APP_COMMAND_H
#define INCLUDED_APP_COMMAND_H

#include "app-movement.h"
#include "app-session.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_command_clear_pending(void);
bool app_command_wait_input(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch);
void app_request_player_command(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_COMMAND_H */
