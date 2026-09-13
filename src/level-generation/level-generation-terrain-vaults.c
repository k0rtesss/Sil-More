#include "angband.h"
#include "externs.h"
#include "level-generation/level-generation-terrain-vaults.h"

/* Zero means unowned; storing serial + 1 also works before the first reset. */
static int vault_ids[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int vault_instances[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int current_instance;
static byte vault_policies[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static char vault_symbols[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

static bool terrain_vault_in_bounds(int y, int x)
{
    return y >= 0 && y < MAX_DUNGEON_HGT
        && x >= 0 && x < MAX_DUNGEON_WID;
}

void terrain_vault_reset(void)
{
    memset(vault_ids, 0, sizeof(vault_ids));
    memset(vault_instances, 0, sizeof(vault_instances));
    current_instance = 0;
    memset(vault_policies, 0, sizeof(vault_policies));
    memset(vault_symbols, 0, sizeof(vault_symbols));
}

void terrain_vault_begin(void)
{
    ++current_instance;
}

void terrain_vault_record(int y, int x, const vault_type* vault, char symbol)
{
    if (!terrain_vault_in_bounds(y, x) || !vault || !v_info
        || symbol == ' ')
        return;

    unsigned policy = 0;
    /* Ordinary structures, including quest rooms, belong to the geology.
     * Types 8/9/10 remain immutable regardless of accidental style flags. */
    if (vault->typ < 8)
    {
        policy = TERRAIN_VAULT_CROSSING | TERRAIN_VAULT_FLOOD;
        if (vault->flags & VLT_TERRAIN_FLOOD)
            policy |= TERRAIN_VAULT_RUINED;
        if (vault->flags & VLT_TERRAIN_REPAIRED)
            policy |= TERRAIN_VAULT_REPAIRED;
    }

    vault_ids[y][x] = (int)(vault - v_info) + 1;
    vault_instances[y][x] = current_instance;
    vault_policies[y][x] = (byte)policy;
    vault_symbols[y][x] = symbol;
}

int terrain_vault_id_at(int y, int x)
{
    return terrain_vault_in_bounds(y, x) ? vault_ids[y][x] - 1 : -1;
}

int terrain_vault_instance_at(int y, int x)
{
    return terrain_vault_in_bounds(y, x) && vault_ids[y][x]
        ? vault_instances[y][x] : -1;
}

unsigned terrain_vault_policy_at(int y, int x)
{
    return terrain_vault_in_bounds(y, x) ? vault_policies[y][x] : 0;
}

char terrain_vault_symbol_at(int y, int x)
{
    return terrain_vault_in_bounds(y, x) && vault_ids[y][x]
        ? vault_symbols[y][x] : ' ';
}
