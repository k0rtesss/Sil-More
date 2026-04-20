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

#include "angband.h"

#include "app-interaction.h"

void app_interaction_clear(app_interaction_state* interaction)
{
    if (!interaction)
        return;

    memset(interaction, 0, sizeof(*interaction));
    interaction->format_version = APP_INTERACTION_FORMAT_VERSION;
    interaction->selected_index = -1;
    interaction->cursor_index = -1;
}

void app_interaction_begin(app_interaction_state* interaction, u16b kind,
    u16b wait_reason)
{
    if (!interaction)
        return;

    app_interaction_clear(interaction);
    interaction->kind = kind;
    interaction->reason = wait_reason;
}

app_interaction_option* app_interaction_append_entry(
    app_interaction_state* interaction)
{
    app_interaction_option* entry;

    if (!interaction || interaction->option_count >= APP_INTERACTION_OPTION_MAX)
        return NULL;

    entry = &interaction->options[interaction->option_count++];
    memset(entry, 0, sizeof(*entry));
    return entry;
}
